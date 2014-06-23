/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "sched_thread.h"
#include "shared_func.h"
#include "pthread_func.h"
#include "logger.h"

volatile bool g_schedule_flag = false;
volatile time_t g_current_time = 0;

static ScheduleArray waiting_schedule_array = {NULL, 0};
static int waiting_del_id = -1;

static int sched_cmp_by_next_call_time(const void *p1, const void *p2)
{
	return ((ScheduleEntry *)p1)->next_call_time - \
			((ScheduleEntry *)p2)->next_call_time;
}

static int sched_init_entries(ScheduleArray *pScheduleArray)
{
	ScheduleEntry *pEntry;
	ScheduleEntry *pEnd;
	time_t time_base;
	struct tm tm_current;
	struct tm tm_base;

	if (pScheduleArray->count < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"schedule count %d < 0",  \
			__LINE__, pScheduleArray->count);
		return EINVAL;
	}
	if (pScheduleArray->count == 0)
	{
		return 0;
	}

	g_current_time = time(NULL);
	localtime_r((time_t *)&g_current_time, &tm_current);
	pEnd = pScheduleArray->entries + pScheduleArray->count;
	for (pEntry=pScheduleArray->entries; pEntry<pEnd; pEntry++)
	{
		if (pEntry->interval <= 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"shedule interval %d <= 0",  \
				__LINE__, pEntry->interval);
			return EINVAL;
		}

		if (pEntry->time_base.hour == TIME_NONE)
		{
			pEntry->next_call_time = g_current_time + \
						pEntry->interval;
		}
		else
		{
			if (tm_current.tm_hour > pEntry->time_base.hour || \
				(tm_current.tm_hour == pEntry->time_base.hour \
				&& tm_current.tm_min >= pEntry->time_base.minute))
			{
				memcpy(&tm_base, &tm_current, sizeof(struct tm));
			}
			else
			{
				time_base = g_current_time - 24 * 3600;
				localtime_r(&time_base, &tm_base);
			}

			tm_base.tm_hour = pEntry->time_base.hour;
			tm_base.tm_min = pEntry->time_base.minute;
			tm_base.tm_sec = 0;
			time_base = mktime(&tm_base);

			pEntry->next_call_time = g_current_time + \
				pEntry->interval - (g_current_time - \
					time_base) % pEntry->interval;
		}

		/*
		{
			char buff1[32];
			char buff2[32];
			logInfo("id=%d, current time=%s, first call time=%s\n", \
				pEntry->id, formatDatetime(g_current_time, \
				"%Y-%m-%d %H:%M:%S", buff1, sizeof(buff1)), \
				formatDatetime(pEntry->next_call_time, \
				"%Y-%m-%d %H:%M:%S", buff2, sizeof(buff2)));
		}
		*/
	}

	return 0;
}

static void sched_make_chain(ScheduleContext *pContext)
{
	ScheduleArray *pScheduleArray;
	ScheduleEntry *pEntry;

	pScheduleArray = &(pContext->scheduleArray);
	if (pScheduleArray->count == 0)
	{
		pContext->head = NULL;
		pContext->tail = NULL;
		return;
	}

	qsort(pScheduleArray->entries, pScheduleArray->count, \
		sizeof(ScheduleEntry), sched_cmp_by_next_call_time);

	pContext->head = pScheduleArray->entries;
	pContext->tail = pScheduleArray->entries + (pScheduleArray->count - 1);
	for (pEntry=pScheduleArray->entries; pEntry<pContext->tail; pEntry++)
	{
		pEntry->next = pEntry + 1;
	}
	pContext->tail->next = NULL;
}

static int sched_check_waiting(ScheduleContext *pContext)
{
	ScheduleArray *pScheduleArray;
	ScheduleEntry *newEntries;
	ScheduleEntry *pWaitingEntry;
	ScheduleEntry *pWaitingEnd;
	ScheduleEntry *pSchedEntry;
	ScheduleEntry *pSchedEnd;
	int allocCount;
	int newCount;
	int result;
	int deleteCount;

	pScheduleArray = &(pContext->scheduleArray);
	deleteCount = 0;
	if (waiting_del_id >= 0)
	{
		pSchedEnd = pScheduleArray->entries + pScheduleArray->count;
		for (pSchedEntry=pScheduleArray->entries; \
			pSchedEntry<pSchedEnd; pSchedEntry++)
		{
			if (pSchedEntry->id == waiting_del_id)
			{
				break;
			}
		}

		if (pSchedEntry < pSchedEnd)
		{
			pSchedEntry++;
			while (pSchedEntry < pSchedEnd)
			{
				memcpy(pSchedEntry - 1, pSchedEntry, \
					sizeof(ScheduleEntry));
				pSchedEntry++;
			}

			deleteCount++;
			pScheduleArray->count--;

			logDebug("file: "__FILE__", line: %d, " \
				"delete task id: %d, " \
				"current schedule count: %d", __LINE__, \
				waiting_del_id, pScheduleArray->count);
		}

		waiting_del_id = -1;
	}

	if (waiting_schedule_array.count == 0)
	{
		if (deleteCount > 0)
		{
			sched_make_chain(pContext);
			return 0;
		}

		return ENOENT;
	}

	allocCount = pScheduleArray->count + waiting_schedule_array.count;
	newEntries = (ScheduleEntry *)malloc(sizeof(ScheduleEntry) * allocCount);
	if (newEntries == NULL)
	{
		result = errno != 0 ? errno : ENOMEM;
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes failed, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(ScheduleEntry) * allocCount, \
			result, STRERROR(result));

		if (deleteCount > 0)
		{
			sched_make_chain(pContext);
		}
		return result;
	}

	if (pScheduleArray->count > 0)
	{
		memcpy(newEntries, pScheduleArray->entries, \
			sizeof(ScheduleEntry) * pScheduleArray->count);
	}
	newCount = pScheduleArray->count;
	pWaitingEnd = waiting_schedule_array.entries + waiting_schedule_array.count;
	for (pWaitingEntry=waiting_schedule_array.entries; \
		pWaitingEntry<pWaitingEnd; pWaitingEntry++)
	{
		pSchedEnd = newEntries + newCount;
		for (pSchedEntry=newEntries; pSchedEntry<pSchedEnd; \
			pSchedEntry++)
		{
			if (pWaitingEntry->id == pSchedEntry->id)
			{
				memcpy(pSchedEntry, pWaitingEntry, \
					sizeof(ScheduleEntry));
				break;
			}
		}

		if (pSchedEntry == pSchedEnd)
		{
			memcpy(pSchedEntry, pWaitingEntry, \
				sizeof(ScheduleEntry));
			newCount++;
		}
	}

	logDebug("file: "__FILE__", line: %d, " \
		"schedule add entries: %d, replace entries: %d", 
		__LINE__, newCount - pScheduleArray->count, \
		waiting_schedule_array.count - (newCount - pScheduleArray->count));

	if (pScheduleArray->entries != NULL)
	{
		free(pScheduleArray->entries);
	}
	pScheduleArray->entries = newEntries;
	pScheduleArray->count = newCount;

	free(waiting_schedule_array.entries);
	waiting_schedule_array.count = 0;
	waiting_schedule_array.entries = NULL;

	sched_make_chain(pContext);

	return 0;
}

static void *sched_thread_entrance(void *args)
{
	ScheduleContext *pContext;
	ScheduleEntry *pPrevious;
	ScheduleEntry *pCurrent;
	ScheduleEntry *pSaveNext;
	ScheduleEntry *pNode;
	ScheduleEntry *pUntil;
	int exec_count;
	int i;
	int sleep_time;

	pContext = (ScheduleContext *)args;
	if (sched_init_entries(&(pContext->scheduleArray)) != 0)
	{
		free(pContext);
		return NULL;
	}
	sched_make_chain(pContext);

	g_schedule_flag = true;
	while (*(pContext->pcontinue_flag))
	{
		sched_check_waiting(pContext);
		if (pContext->scheduleArray.count == 0)  //no schedule entry
		{
			sleep(1);
			g_current_time = time(NULL);
			continue;
		}

		g_current_time = time(NULL);
		sleep_time = pContext->head->next_call_time - g_current_time;

		/*
		//fprintf(stderr, "count=%d, sleep_time=%d\n", \
			pContext->scheduleArray.count, sleep_time);
		*/
		while (sleep_time > 0 && *(pContext->pcontinue_flag))
		{
			sleep(1);
			g_current_time = time(NULL);
			if (sched_check_waiting(pContext) == 0)
			{
				break;
			}
			sleep_time--;
		}

		if (!(*(pContext->pcontinue_flag)))
		{
			break;
		}

		exec_count = 0;
		pCurrent = pContext->head;
		while (*(pContext->pcontinue_flag) && (pCurrent != NULL \
			&& pCurrent->next_call_time <= g_current_time))
		{
			//fprintf(stderr, "exec task id=%d\n", pCurrent->id);
			pCurrent->task_func(pCurrent->func_args);
			pCurrent->next_call_time = g_current_time + \
						pCurrent->interval;
			pCurrent = pCurrent->next;
			exec_count++;
		}

		if (exec_count == 0 || pContext->scheduleArray.count == 1)
		{
			continue;
		}

		if (exec_count > pContext->scheduleArray.count / 2)
		{
			sched_make_chain(pContext);
			continue;
		}

		pNode = pContext->head;
		pContext->head = pCurrent;  //new chain head
		for (i=0; i<exec_count; i++)
		{
			if (pNode->next_call_time >= pContext->tail->next_call_time)
			{
				pContext->tail->next = pNode;
				pContext->tail = pNode;
				pNode = pNode->next;
				pContext->tail->next = NULL;
				continue;
			}

			pPrevious = NULL;
			pUntil = pContext->head;
			while (pUntil != NULL && \
				pNode->next_call_time > pUntil->next_call_time)
			{
				pPrevious = pUntil;
				pUntil = pUntil->next;
			}

			pSaveNext = pNode->next;
			if (pPrevious == NULL)
			{
				pContext->head = pNode;
			}
			else
			{
				pPrevious->next = pNode;
			}
			pNode->next = pUntil;

			pNode = pSaveNext;
		}
	}

	g_schedule_flag = false;

	logDebug("file: "__FILE__", line: %d, " \
		"schedule thread exit", __LINE__);

	free(pContext);
	return NULL;
}

static int sched_dup_array(const ScheduleArray *pSrcArray, \
		ScheduleArray *pDestArray)
{
	int result;
	int bytes;

	if (pSrcArray->count == 0)
	{
		pDestArray->entries = NULL;
		pDestArray->count = 0;
		return 0;
	}

	bytes = sizeof(ScheduleEntry) * pSrcArray->count;
	pDestArray->entries = (ScheduleEntry *)malloc(bytes);
	if (pDestArray->entries == NULL)
	{
		result = errno != 0 ? errno : ENOMEM;
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes failed, " \
			"errno: %d, error info: %s", \
			__LINE__, bytes, result, STRERROR(result));
		return result;
	}

	memcpy(pDestArray->entries, pSrcArray->entries, bytes);
	pDestArray->count = pSrcArray->count;
	return 0;
}

int sched_add_entries(const ScheduleArray *pScheduleArray)
{
	int result;

	if (pScheduleArray->count == 0)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"no schedule entry", __LINE__);
		return ENOENT;
	}

	while (waiting_schedule_array.entries != NULL)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"waiting for schedule array ready ...", __LINE__);
		sleep(1);
	}

	if ((result=sched_dup_array(pScheduleArray, &waiting_schedule_array))!=0)
	{
		return result;
	}

	return sched_init_entries(&waiting_schedule_array);
}

int sched_del_entry(const int id)
{
	if (id < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"id: %d is invalid!", __LINE__, id);
		return EINVAL;
	}

	while (waiting_del_id >= 0)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"waiting for delete ready ...", __LINE__);
		sleep(1);
	}

	waiting_del_id = id;
	return 0;
}

int sched_start(ScheduleArray *pScheduleArray, pthread_t *ptid, \
		const int stack_size, bool * volatile pcontinue_flag)
{
	int result;
	pthread_attr_t thread_attr;
	ScheduleContext *pContext;

	pContext = (ScheduleContext *)malloc(sizeof(ScheduleContext));
	if (pContext == NULL)
	{
		result = errno != 0 ? errno : ENOMEM;
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes failed, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(ScheduleContext), \
			result, STRERROR(result));
		return result;
	}

	if ((result=init_pthread_attr(&thread_attr, stack_size)) != 0)
	{
		free(pContext);
		return result;
	}

	if ((result=sched_dup_array(pScheduleArray, \
			&(pContext->scheduleArray))) != 0)
	{
		free(pContext);
		return result;
	}

	pContext->pcontinue_flag = pcontinue_flag;
	if ((result=pthread_create(ptid, &thread_attr, \
		sched_thread_entrance, pContext)) != 0)
	{
		free(pContext);
		logError("file: "__FILE__", line: %d, " \
			"create thread failed, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	pthread_attr_destroy(&thread_attr);
	return result;
}

