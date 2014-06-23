//fast_mblock.c

#include <errno.h>
#include <sys/resource.h>
#include <pthread.h>
#include "fast_mblock.h"
#include "logger.h"
#include "shared_func.h"
#include "pthread_func.h"

int fast_mblock_init(struct fast_mblock_man *mblock, const int element_size, \
		const int alloc_elements_once)
{
	int result;

	if (element_size <= 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"invalid block size: %d", \
			__LINE__, element_size);
		return EINVAL;
	}

	mblock->element_size = element_size;
	if (alloc_elements_once > 0)
	{
		mblock->alloc_elements_once = alloc_elements_once;
	}
	else
	{
		int block_size;
		block_size = MEM_ALIGN(sizeof(struct fast_mblock_node) \
			+ element_size);
		mblock->alloc_elements_once = (1024 * 1024) / block_size;
	}

	if ((result=init_pthread_lock(&(mblock->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"init_pthread_lock fail, errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	mblock->malloc_chain_head = NULL;
	mblock->free_chain_head = NULL;

	return 0;
}

static int fast_mblock_prealloc(struct fast_mblock_man *mblock)
{
	struct fast_mblock_node *pNode;
	struct fast_mblock_malloc *pMallocNode;
	char *pNew;
	char *pTrunkStart;
	char *p;
	char *pLast;
	int block_size;
	int alloc_size;

	block_size = MEM_ALIGN(sizeof(struct fast_mblock_node) + \
			mblock->element_size);
	alloc_size = sizeof(struct fast_mblock_malloc) + block_size * \
			mblock->alloc_elements_once;

	pNew = (char *)malloc(alloc_size);
	if (pNew == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, alloc_size, errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	memset(pNew, 0, alloc_size);

	pMallocNode = (struct fast_mblock_malloc *)pNew;

	pTrunkStart = pNew + sizeof(struct fast_mblock_malloc);
	pLast = pNew + (alloc_size - block_size);
	for (p=pTrunkStart; p<pLast; p += block_size)
	{
		pNode = (struct fast_mblock_node *)p;
		pNode->next = (struct fast_mblock_node *)(p + block_size);
	}

	((struct fast_mblock_node *)pLast)->next = NULL;
	mblock->free_chain_head = (struct fast_mblock_node *)pTrunkStart;

	pMallocNode->next = mblock->malloc_chain_head;
	mblock->malloc_chain_head = pMallocNode;

	return 0;
}

void fast_mblock_destroy(struct fast_mblock_man *mblock)
{
	struct fast_mblock_malloc *pMallocNode;
	struct fast_mblock_malloc *pMallocTmp;

	if (mblock->malloc_chain_head == NULL)
	{
		return;
	}

	pMallocNode = mblock->malloc_chain_head;
	while (pMallocNode != NULL)
	{
		pMallocTmp = pMallocNode;
		pMallocNode = pMallocNode->next;

		free(pMallocTmp);
	}
	mblock->malloc_chain_head = NULL;
	mblock->free_chain_head = NULL;

	pthread_mutex_destroy(&(mblock->lock));
}

struct fast_mblock_node *fast_mblock_alloc(struct fast_mblock_man *mblock)
{
	struct fast_mblock_node *pNode;
	int result;

	if ((result=pthread_mutex_lock(&(mblock->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return NULL;
	}

	if (mblock->free_chain_head != NULL)
	{
		pNode = mblock->free_chain_head;
		mblock->free_chain_head = pNode->next;
	}
	else
	{
		if ((result=fast_mblock_prealloc(mblock)) == 0)
		{
			pNode = mblock->free_chain_head;
			mblock->free_chain_head = pNode->next;
		}
		else
		{
			pNode = NULL;
		}
	}

	if ((result=pthread_mutex_unlock(&(mblock->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return pNode;
}

int fast_mblock_free(struct fast_mblock_man *mblock, \
		     struct fast_mblock_node *pNode)
{
	int result;

	if ((result=pthread_mutex_lock(&(mblock->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	pNode->next = mblock->free_chain_head;
	mblock->free_chain_head = pNode;

	if ((result=pthread_mutex_unlock(&(mblock->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return 0;
}

int fast_mblock_count(struct fast_mblock_man *mblock)
{
	struct fast_mblock_node *pNode;
	int count;
	int result;

	if ((result=pthread_mutex_lock(&(mblock->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return -1;
	}

	count = 0;
	pNode = mblock->free_chain_head;
	while (pNode != NULL)
	{
		pNode = pNode->next;
		count++;
	}

	if ((result=pthread_mutex_unlock(&(mblock->lock))) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return count;
}

