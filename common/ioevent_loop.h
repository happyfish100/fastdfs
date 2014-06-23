#ifndef _IOEVENT_LOOP_H
#define _IOEVENT_LOOP_H

#include "fast_task_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

int ioevent_loop(struct nio_thread_data *pThreadData,
	IOEventCallback recv_notify_callback, TaskCleanUpCallBack
	clean_up_callback, volatile bool *continue_flag);

int ioevent_set(struct fast_task_info *pTask, struct nio_thread_data *pThread,
	int sock, short event, IOEventCallback callback, const int timeout);

#ifdef __cplusplus
}
#endif

#endif

