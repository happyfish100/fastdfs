/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#ifndef _SCHED_THREAD_H_
#define _SCHED_THREAD_H_

#include <time.h>
#include "common_define.h"

typedef int (*TaskFunc) (void *args);

typedef struct tagScheduleEntry
{
	int id;  //the task id

	/* the time base to execute task, such as 00:00, interval is 3600,
           means execute the task every hour as 1:00, 2:00, 3:00 etc. */
	TimeInfo time_base;

	int interval;   //the interval for execute task, unit is second

	TaskFunc task_func; //callback function
	void *func_args;    //arguments pass to callback function

	/* following are internal fields, do not set manually! */
	time_t next_call_time;  
	struct tagScheduleEntry *next;
} ScheduleEntry;

typedef struct
{
	ScheduleEntry *entries;
	int count;
} ScheduleArray;

typedef struct
{
	ScheduleArray scheduleArray;
	ScheduleEntry *head;  //schedule chain head
        ScheduleEntry *tail;  //schedule chain tail
	bool *pcontinue_flag;
} ScheduleContext;

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool g_schedule_flag; //schedule continue running flag
extern volatile time_t g_current_time;  //the current time


#define get_current_time() (g_schedule_flag ? g_current_time: time(NULL))

int sched_add_entries(const ScheduleArray *pScheduleArray);
int sched_del_entry(const int id);

/** execute the schedule thread
 *  parameters:
 *  	     pScheduleArray: schedule task
 *  	     ptid: store the schedule thread id
 *  	     stack_size: set thread stack size (byes)
 *  	     pcontinue_flag: main process continue running flag
 * return: error no, 0 for success, != 0 fail
*/
int sched_start(ScheduleArray *pScheduleArray, pthread_t *ptid, \
		const int stack_size, bool * volatile pcontinue_flag);

#ifdef __cplusplus
}
#endif

#endif

