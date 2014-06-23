//fast_task_queue.c

#include <errno.h>
#include <sys/resource.h>
#include <pthread.h>
#include "fast_task_queue.h"
#include "logger.h"
#include "shared_func.h"
#include "pthread_func.h"

static struct fast_task_queue g_free_queue;

struct mpool_chain {
	struct fast_task_info *blocks;
	struct fast_task_info *last_block;   //last block
	struct mpool_chain *next;
} *g_mpool = NULL;

#define ALIGNED_TASK_INFO_SIZE  MEM_ALIGN(sizeof(struct fast_task_info))

int task_queue_init(struct fast_task_queue *pQueue)
{
	int result;

	if ((result=init_pthread_lock(&(pQueue->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"init_pthread_lock fail, errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	pQueue->head = NULL;
	pQueue->tail = NULL;

	return 0;
}

static struct mpool_chain *malloc_mpool(const int block_size, \
		const int total_alloc_size)
{
	struct fast_task_info *pTask;
	char *p;
	char *pCharEnd;
	struct mpool_chain *mpool;

	mpool = (struct mpool_chain *)malloc(sizeof(struct mpool_chain));
	if (mpool == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(struct mpool_chain), \
			errno, STRERROR(errno));
		return NULL;
	}

	mpool->next = NULL;
	mpool->blocks = (struct fast_task_info *)malloc(total_alloc_size);
	if (mpool->blocks == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, total_alloc_size, \
			errno, STRERROR(errno));
		free(mpool);
		return NULL;
	}
	memset(mpool->blocks, 0, total_alloc_size);

	pCharEnd = ((char *)mpool->blocks) + total_alloc_size;
	for (p=(char *)mpool->blocks; p<pCharEnd; p += block_size)
	{
		pTask = (struct fast_task_info *)p;
		pTask->size = g_free_queue.min_buff_size;

		pTask->arg = p + ALIGNED_TASK_INFO_SIZE;
		if (g_free_queue.malloc_whole_block)
		{
			pTask->data = (char *)pTask->arg + \
					g_free_queue.arg_size;
		}
		else
		{
			pTask->data = (char *)malloc(pTask->size);
			if (pTask->data == NULL)
			{
				char *pt;

				logError("file: "__FILE__", line: %d, " \
					"malloc %d bytes fail, " \
					"errno: %d, error info: %s", \
					__LINE__, pTask->size, \
					errno, STRERROR(errno));

				for (pt=(char *)mpool->blocks; pt < p; \
					pt += block_size)
				{
					free(((struct fast_task_info *)pt)->data);
				}

				free(mpool->blocks);
				free(mpool);
				return NULL;
			}
		}
	}

	mpool->last_block = (struct fast_task_info *)(pCharEnd - block_size);
	for (p=(char *)mpool->blocks; p<(char *)mpool->last_block; p += block_size)
	{
		pTask = (struct fast_task_info *)p;
		pTask->next = (struct fast_task_info *)(p + block_size);
	}
	mpool->last_block->next = NULL;

	return mpool;
}

int free_queue_init(const int max_connections, const int min_buff_size, \
		const int max_buff_size, const int arg_size)
{
	int64_t total_size;
	struct mpool_chain *mpool;
	int block_size;
	int alloc_size;
	int result;
	int loop_count;
	int aligned_min_size;
	int aligned_max_size;
	int aligned_arg_size;
	rlim_t max_data_size;

	if ((result=init_pthread_lock(&(g_free_queue.lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"init_pthread_lock fail, errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	aligned_min_size = MEM_ALIGN(min_buff_size);
	aligned_max_size = MEM_ALIGN(max_buff_size);
	aligned_arg_size = MEM_ALIGN(arg_size);
	block_size = ALIGNED_TASK_INFO_SIZE + aligned_arg_size;
	alloc_size = block_size * max_connections;
	if (aligned_max_size > aligned_min_size)
	{
		total_size = alloc_size;
		g_free_queue.malloc_whole_block = false;
		max_data_size = 0;
	}
	else
	{
		struct rlimit rlimit_data;

		if (getrlimit(RLIMIT_DATA, &rlimit_data) < 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"call getrlimit fail, " \
				"errno: %d, error info: %s", \
				__LINE__, errno, STRERROR(errno));
			return errno != 0 ? errno : EPERM;
		}
		if (rlimit_data.rlim_cur == RLIM_INFINITY)
		{
			max_data_size = 256 * 1024 * 1024;
		}
		else
		{
			max_data_size = rlimit_data.rlim_cur;
			if (max_data_size > 256 * 1024 * 1024)
			{
				max_data_size = 256 * 1024 * 1024;
			}
		}

		if (max_data_size >= (int64_t)(block_size + aligned_min_size) *
			(int64_t)max_connections)
		{
			total_size = alloc_size + (int64_t)aligned_min_size *
					max_connections;
			g_free_queue.malloc_whole_block = true;
			block_size += aligned_min_size;
		}
		else
		{
			total_size = alloc_size;
			g_free_queue.malloc_whole_block = false;
			max_data_size = 0;
		}
	}

	logDebug("file: "__FILE__", line: %d, " \
		"max_connections: %d, min_buff_size: %d, max_buff_size: %d, " \
		"block_size: %d, arg_size: %d, max_data_size: %d, " \
		"total_size: "INT64_PRINTF_FORMAT, __LINE__, \
		max_connections, aligned_min_size, aligned_max_size, \
		block_size, aligned_arg_size, (int)max_data_size, total_size);

	g_free_queue.max_connections = max_connections;
	g_free_queue.min_buff_size = aligned_min_size;
	g_free_queue.max_buff_size = aligned_max_size;
	g_free_queue.arg_size = aligned_arg_size;

	if ((!g_free_queue.malloc_whole_block) || \
		(total_size <= max_data_size))
	{
		loop_count = 1;
		mpool = malloc_mpool(block_size, total_size);
		if (mpool == NULL)
		{
			return errno != 0 ? errno : ENOMEM;
		}
		g_mpool = mpool;
	}
	else
	{
		struct mpool_chain *previous_mpool;
		int remain_count;
		int alloc_once;
		int current_count;
		int current_alloc_size;

		mpool = NULL;
		previous_mpool = NULL;
		loop_count = 0;
		remain_count = max_connections;
		alloc_once = max_data_size / block_size;
		while (remain_count > 0)
		{
			current_count = (remain_count > alloc_once) ? \
					alloc_once : remain_count;
			current_alloc_size = block_size * current_count;
			mpool = malloc_mpool(block_size, current_alloc_size);
			if (mpool == NULL)
			{
				free_queue_destroy();
				return errno != 0 ? errno : ENOMEM;
			}

			if (previous_mpool == NULL)
			{
				g_mpool = mpool;
			}
			else
			{
				previous_mpool->next = mpool;
				previous_mpool->last_block->next = mpool->blocks;
			}
			previous_mpool = mpool;

			remain_count -= current_count;
			loop_count++;
		}

		logDebug("file: "__FILE__", line: %d, " \
			"alloc_once: %d", __LINE__, alloc_once);
	}

	logDebug("file: "__FILE__", line: %d, " \
		"malloc task info as whole: %d, malloc loop count: %d", \
		__LINE__, g_free_queue.malloc_whole_block, loop_count);

	if (g_mpool != NULL)
	{
		g_free_queue.head = g_mpool->blocks;
		g_free_queue.tail = mpool->last_block;

		/*
		struct fast_task_info *pTask;
		int task_count = 0;

		pTask = g_free_queue.head;
		while (pTask != NULL)
		{
			task_count++;
			pTask = pTask->next;
		}
		logDebug("file: "__FILE__", line: %d, " \
			"task count: %d", __LINE__, task_count);
		*/
	}

	return 0;
}

void free_queue_destroy()
{
	struct mpool_chain *mpool;
	struct mpool_chain *mp;

	if (g_mpool == NULL)
	{
		return;
	}

	if (!g_free_queue.malloc_whole_block)
	{
		char *p;
		char *pCharEnd;
		int block_size;
		struct fast_task_info *pTask;

		block_size = ALIGNED_TASK_INFO_SIZE + g_free_queue.arg_size;
		pCharEnd = ((char *)g_mpool->blocks) + block_size * \
				g_free_queue.max_connections;
		for (p=(char *)g_mpool->blocks; p<pCharEnd; p += block_size)
		{
			pTask = (struct fast_task_info *)p;
			if (pTask->data != NULL)
			{
				free(pTask->data);
				pTask->data = NULL;
			}
		}
	}

	mpool = g_mpool;
	while (mpool != NULL)
	{
		mp = mpool;
		mpool = mpool->next;

		free(mp->blocks);
		free(mp);
	}
	g_mpool = NULL;

	pthread_mutex_destroy(&(g_free_queue.lock));
}

struct fast_task_info *free_queue_pop()
{
	return task_queue_pop(&g_free_queue);;
}

int free_queue_push(struct fast_task_info *pTask)
{
	char *new_buff;
	int result;

	*(pTask->client_ip) = '\0';
	pTask->length = 0;
	pTask->offset = 0;
	pTask->req_count = 0;

	if (pTask->size > g_free_queue.min_buff_size) //need thrink
	{
		new_buff = (char *)malloc(g_free_queue.min_buff_size);
		if (new_buff == NULL)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s", \
				__LINE__, g_free_queue.min_buff_size, \
				errno, STRERROR(errno));
		}
		else
		{
			free(pTask->data);
			pTask->size = g_free_queue.min_buff_size;
			pTask->data = new_buff;
		}
	}

	if ((result=pthread_mutex_lock(&g_free_queue.lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	pTask->next = g_free_queue.head;
	g_free_queue.head = pTask;
	if (g_free_queue.tail == NULL)
	{
		g_free_queue.tail = pTask;
	}

	if ((result=pthread_mutex_unlock(&g_free_queue.lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return result;
}

int free_queue_count()
{
	return task_queue_count(&g_free_queue);
}

int task_queue_push(struct fast_task_queue *pQueue, \
		struct fast_task_info *pTask)
{
	int result;

	if ((result=pthread_mutex_lock(&(pQueue->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	pTask->next = NULL;
	if (pQueue->tail == NULL)
	{
		pQueue->head = pTask;
	}
	else
	{
		pQueue->tail->next = pTask;
	}
	pQueue->tail = pTask;

	if ((result=pthread_mutex_unlock(&(pQueue->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return 0;
}

struct fast_task_info *task_queue_pop(struct fast_task_queue *pQueue)
{
	struct fast_task_info *pTask;
	int result;

	if ((result=pthread_mutex_lock(&(pQueue->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return NULL;
	}

	pTask = pQueue->head;
	if (pTask != NULL)
	{
		pQueue->head = pTask->next;
		if (pQueue->head == NULL)
		{
			pQueue->tail = NULL;
		}
	}

	if ((result=pthread_mutex_unlock(&(pQueue->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return pTask;
}

int task_queue_count(struct fast_task_queue *pQueue)
{
	struct fast_task_info *pTask;
	int count;
	int result;

	if ((result=pthread_mutex_lock(&(pQueue->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return 0;
	}

	count = 0;
	pTask = pQueue->head;
	while (pTask != NULL)
	{
		pTask = pTask->next;
		count++;
	}

	if ((result=pthread_mutex_unlock(&(pQueue->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return count;
}

