/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <pthread.h>
#include "shared_func.h"
#include "pthread_func.h"
#include "sched_thread.h"
#include "logger.h"

#ifndef LINE_MAX
#define LINE_MAX 2048
#endif

#define LOG_BUFF_SIZE    64 * 1024

LogContext g_log_context = {LOG_INFO, STDERR_FILENO, NULL};

static int log_fsync(LogContext *pContext, const bool bNeedLock);

static int check_and_mk_log_dir(const char *base_path)
{
	char data_path[MAX_PATH_SIZE];

	snprintf(data_path, sizeof(data_path), "%s/logs", base_path);
	if (!fileExists(data_path))
	{
		if (mkdir(data_path, 0755) != 0)
		{
			fprintf(stderr, "mkdir \"%s\" fail, " \
				"errno: %d, error info: %s", \
				data_path, errno, STRERROR(errno));
			return errno != 0 ? errno : EPERM;
		}
	}

	return 0;
}

int log_init()
{
	if (g_log_context.log_buff != NULL)
	{
		return 0;
	}

	return log_init_ex(&g_log_context);
}

int log_init_ex(LogContext *pContext)
{
	int result;

	memset(pContext, 0, sizeof(LogContext));
	pContext->log_level = LOG_INFO;
	pContext->log_fd = STDERR_FILENO;
	pContext->log_to_cache = false;
	pContext->rotate_immediately = false;
	pContext->time_precision = LOG_TIME_PRECISION_SECOND;

	pContext->log_buff = (char *)malloc(LOG_BUFF_SIZE);
	if (pContext->log_buff == NULL)
	{
		fprintf(stderr, "malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			LOG_BUFF_SIZE, errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	pContext->pcurrent_buff = pContext->log_buff;

	if ((result=init_pthread_lock(&(pContext->log_thread_lock))) != 0)
	{
		return result;
	}

	return 0;
}

static int log_open(LogContext *pContext)
{
	if ((pContext->log_fd = open(pContext->log_filename, O_WRONLY | \
				O_CREAT | O_APPEND, 0644)) < 0)
	{
		fprintf(stderr, "open log file \"%s\" to write fail, " \
			"errno: %d, error info: %s", \
			pContext->log_filename, errno, STRERROR(errno));
		pContext->log_fd = STDERR_FILENO;
		return errno != 0 ? errno : EACCES;
	}

	pContext->current_size = lseek(pContext->log_fd, 0, SEEK_END);
	if (pContext->current_size < 0)
	{
		fprintf(stderr, "lseek file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			pContext->log_filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EACCES;
	}

	return 0;
}

int log_set_prefix_ex(LogContext *pContext, const char *base_path, \
		const char *filename_prefix)
{
	int result;

	if ((result=check_and_mk_log_dir(base_path)) != 0)
	{
		return result;
	}

	snprintf(pContext->log_filename, MAX_PATH_SIZE, "%s/logs/%s.log", \
		base_path, filename_prefix);

	return log_open(pContext);
}

int log_set_filename_ex(LogContext *pContext, const char *log_filename)
{
	snprintf(pContext->log_filename, MAX_PATH_SIZE, "%s", log_filename);
	return log_open(pContext);
}

void log_set_cache_ex(LogContext *pContext, const bool bLogCache)
{
	pContext->log_to_cache = bLogCache;
}

void log_set_time_precision(LogContext *pContext, const int time_precision)
{
	pContext->time_precision = time_precision;
}

void log_destroy_ex(LogContext *pContext)
{
	if (pContext->log_fd >= 0 && pContext->log_fd != STDERR_FILENO)
	{
		log_fsync(pContext, true);

		close(pContext->log_fd);
		pContext->log_fd = STDERR_FILENO;

		pthread_mutex_destroy(&pContext->log_thread_lock);
	}

	if (pContext->log_buff != NULL)
	{
		free(pContext->log_buff);
		pContext->log_buff = NULL;
		pContext->pcurrent_buff = NULL;
	}
}

int log_sync_func(void *args)
{
	if (args == NULL)
	{
		return EINVAL;
	}

	return log_fsync((LogContext *)args, true);
}

int log_notify_rotate(void *args)
{
	if (args == NULL)
	{
		return EINVAL;
	}

	((LogContext *)args)->rotate_immediately = true;
	return 0;
}

static int log_rotate(LogContext *pContext)
{
	struct tm tm;
	time_t current_time;
	char new_filename[MAX_PATH_SIZE + 32];

	if (*(pContext->log_filename) == '\0')
	{
		return ENOENT;
	}

	close(pContext->log_fd);

	current_time = get_current_time();
	localtime_r(&current_time, &tm);
	sprintf(new_filename, "%s.%04d%02d%02d_%02d%02d%02d", \
			pContext->log_filename, \
			tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, \
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (rename(pContext->log_filename, new_filename) != 0)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"rename %s to %s fail, errno: %d, error info: %s", \
			__LINE__, pContext->log_filename, new_filename, \
			errno, STRERROR(errno));
	}

	return log_open(pContext);
}

static int log_check_rotate(LogContext *pContext, const bool bNeedLock)
{
	int result;

	if (pContext->log_fd == STDERR_FILENO)
	{
		if (pContext->current_size > 0)
		{
			pContext->current_size = 0;
		}
		return ENOENT;
	}
	
	if (bNeedLock)
	{
		pthread_mutex_lock(&(pContext->log_thread_lock));
	}

	if (pContext->rotate_immediately)
	{
		result = log_rotate(pContext);
		pContext->rotate_immediately = false;
	}
	else
	{
		result = 0;
	}

	if (bNeedLock)
	{
		pthread_mutex_unlock(&(pContext->log_thread_lock));
	}

	return result;
}

static int log_fsync(LogContext *pContext, const bool bNeedLock)
{
	int result;
	int lock_res;
	int write_bytes;

	write_bytes = pContext->pcurrent_buff - pContext->log_buff;
	if (write_bytes == 0)
	{
		if (!pContext->rotate_immediately)
		{
			return 0;
		}
		else
		{
			return log_check_rotate(pContext, bNeedLock);
		}
	}

	if (bNeedLock && ((lock_res=pthread_mutex_lock( \
			&(pContext->log_thread_lock))) != 0))
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, lock_res, STRERROR(lock_res));
	}

	if (pContext->rotate_size > 0)
	{
		pContext->current_size += write_bytes;
		if (pContext->current_size > pContext->rotate_size)
		{
			pContext->rotate_immediately = true;
			log_check_rotate(pContext, false);
		}
	}

	result = 0;
	do
	{
	write_bytes = pContext->pcurrent_buff - pContext->log_buff;
	if (write(pContext->log_fd, pContext->log_buff, write_bytes) != \
		write_bytes)
	{
		result = errno != 0 ? errno : EIO;
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"call write fail, errno: %d, error info: %s\n",\
			 __LINE__, result, STRERROR(result));
		break;
	}

	if (pContext->log_fd != STDERR_FILENO)
	{
		if (fsync(pContext->log_fd) != 0)
		{
			result = errno != 0 ? errno : EIO;
			fprintf(stderr, "file: "__FILE__", line: %d, " \
				"call fsync fail, errno: %d, error info: %s\n",\
				 __LINE__, result, STRERROR(result));
			break;
		}
	}

	if (pContext->rotate_immediately)
	{
		result = log_check_rotate(pContext, false);
	}
	} while (0);

	pContext->pcurrent_buff = pContext->log_buff;
	if (bNeedLock && ((lock_res=pthread_mutex_unlock( \
			&(pContext->log_thread_lock))) != 0))
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, lock_res, STRERROR(lock_res));
	}

	return result;
}

static void doLogEx(LogContext *pContext, struct timeval *tv, \
		const char *caption, const char *text, const int text_len, \
		const bool bNeedSync)
{
	struct tm tm;
	int time_fragment;
	int buff_len;
	int result;

	if (pContext->time_precision == LOG_TIME_PRECISION_SECOND)
	{
		time_fragment = 0;
	}
	else
	{
		if (pContext->time_precision == LOG_TIME_PRECISION_MSECOND)
		{
			time_fragment = tv->tv_usec / 1000;
		}
		else
		{
			time_fragment = tv->tv_usec;
		}
	}

	localtime_r(&tv->tv_sec, &tm);
	if ((result=pthread_mutex_lock(&pContext->log_thread_lock)) != 0)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if (text_len + 64 > LOG_BUFF_SIZE)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"log buff size: %d < log text length: %d ", \
			__LINE__, LOG_BUFF_SIZE, text_len + 64);
		pthread_mutex_unlock(&(pContext->log_thread_lock));
		return;
	}

	if ((pContext->pcurrent_buff - pContext->log_buff) + text_len + 64 \
			> LOG_BUFF_SIZE)
	{
		log_fsync(pContext, false);
	}

	if (pContext->time_precision == LOG_TIME_PRECISION_SECOND)
	{
		buff_len = sprintf(pContext->pcurrent_buff, \
			"[%04d-%02d-%02d %02d:%02d:%02d] ", \
			tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, \
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	}
	else
	{
		buff_len = sprintf(pContext->pcurrent_buff, \
			"[%04d-%02d-%02d %02d:%02d:%02d.%03d] ", \
			tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, \
			tm.tm_hour, tm.tm_min, tm.tm_sec, time_fragment);
	}
	pContext->pcurrent_buff += buff_len;

	if (caption != NULL)
	{
		buff_len = sprintf(pContext->pcurrent_buff, "%s - ", caption);
		pContext->pcurrent_buff += buff_len;
	}
	memcpy(pContext->pcurrent_buff, text, text_len);
	pContext->pcurrent_buff += text_len;
	*pContext->pcurrent_buff++ = '\n';

	if (!pContext->log_to_cache || bNeedSync)
	{
		log_fsync(pContext, false);
	}

	if ((result=pthread_mutex_unlock(&(pContext->log_thread_lock))) != 0)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}
}

static void doLog(LogContext *pContext, const char *caption, \
		const char *text, const int text_len, const bool bNeedSync)
{
	struct timeval tv;

	if (pContext->time_precision == LOG_TIME_PRECISION_SECOND)
	{
		tv.tv_sec = get_current_time();
		tv.tv_usec = 0;
	}
	else
	{
		gettimeofday(&tv, NULL);
	}

	doLogEx(pContext, &tv, caption, text, text_len, bNeedSync);
}

void log_it_ex1(LogContext *pContext, const int priority, \
		const char *text, const int text_len)
{
	bool bNeedSync;
	char *caption;

	switch(priority)
	{
		case LOG_DEBUG:
			bNeedSync = true;
			caption = "DEBUG";
			break;
		case LOG_INFO:
			bNeedSync = true;
			caption = "INFO";
			break;
		case LOG_NOTICE:
			bNeedSync = false;
			caption = "NOTICE";
			break;
		case LOG_WARNING:
			bNeedSync = false;
			caption = "WARNING";
			break;
		case LOG_ERR:
			bNeedSync = false;
			caption = "ERROR";
			break;
		case LOG_CRIT:
			bNeedSync = true;
			caption = "CRIT";
			break;
		case LOG_ALERT:
			bNeedSync = true;
			caption = "ALERT";
			break;
		case LOG_EMERG:
			bNeedSync = true;
			caption = "EMERG";
			break;
		default:
			bNeedSync = false;
			caption = "UNKOWN";
			break;
	}

	doLog(pContext, caption, text, text_len, bNeedSync);
}

void log_it_ex(LogContext *pContext, const int priority, const char *format, ...)
{
	bool bNeedSync;
	char text[LINE_MAX];
	char *caption;
	int len;

	va_list ap;
	va_start(ap, format);
	len = vsnprintf(text, sizeof(text), format, ap);
	va_end(ap);

	switch(priority)
	{
		case LOG_DEBUG:
			bNeedSync = true;
			caption = "DEBUG";
			break;
		case LOG_INFO:
			bNeedSync = true;
			caption = "INFO";
			break;
		case LOG_NOTICE:
			bNeedSync = false;
			caption = "NOTICE";
			break;
		case LOG_WARNING:
			bNeedSync = false;
			caption = "WARNING";
			break;
		case LOG_ERR:
			bNeedSync = false;
			caption = "ERROR";
			break;
		case LOG_CRIT:
			bNeedSync = true;
			caption = "CRIT";
			break;
		case LOG_ALERT:
			bNeedSync = true;
			caption = "ALERT";
			break;
		case LOG_EMERG:
			bNeedSync = true;
			caption = "EMERG";
			break;
		default:
			bNeedSync = false;
			caption = "UNKOWN";
			break;
	}

	doLog(pContext, caption, text, len, bNeedSync);
}


#define _DO_LOG(pContext, priority, caption, bNeedSync) \
	char text[LINE_MAX]; \
	int len; \
\
	if (pContext->log_level < priority) \
	{ \
		return; \
	} \
\
	{ \
	va_list ap; \
	va_start(ap, format); \
	len = vsnprintf(text, sizeof(text), format, ap);  \
	va_end(ap); \
	} \
\
	doLog(pContext, caption, text, len, bNeedSync); \


void logEmergEx(LogContext *pContext, const char *format, ...)
{
	_DO_LOG(pContext, LOG_EMERG, "EMERG", true)
}

void logAlertEx(LogContext *pContext, const char *format, ...)
{
	_DO_LOG(pContext, LOG_ALERT, "ALERT", true)
}

void logCritEx(LogContext *pContext, const char *format, ...)
{
	_DO_LOG(pContext, LOG_CRIT, "CRIT", true)
}

void logErrorEx(LogContext *pContext, const char *format, ...)
{
	_DO_LOG(pContext, LOG_ERR, "ERROR", false)
}

void logWarningEx(LogContext *pContext, const char *format, ...)
{
	_DO_LOG(pContext, LOG_WARNING, "WARNING", false)
}

void logNoticeEx(LogContext *pContext, const char *format, ...)
{
	_DO_LOG(pContext, LOG_NOTICE, "NOTICE", false)
}

void logInfoEx(LogContext *pContext, const char *format, ...)
{
	_DO_LOG(pContext, LOG_INFO, "INFO", false)
}

void logDebugEx(LogContext *pContext, const char *format, ...)
{
	_DO_LOG(pContext, LOG_DEBUG, "DEBUG", false)
}

void logAccess(LogContext *pContext, struct timeval *tvStart, \
		const char *format, ...)
{
	char text[LINE_MAX];
	int len;
	va_list ap;

	va_start(ap, format);
	len = vsnprintf(text, sizeof(text), format, ap);
	va_end(ap);

	doLogEx(pContext, tvStart, NULL, text, len, false);
}

#ifndef LOG_FORMAT_CHECK

void logEmerg(const char *format, ...)
{
	_DO_LOG((&g_log_context), LOG_EMERG, "EMERG", true)
}

void logAlert(const char *format, ...)
{
	_DO_LOG((&g_log_context), LOG_ALERT, "ALERT", true)
}

void logCrit(const char *format, ...)
{
	_DO_LOG((&g_log_context), LOG_CRIT, "CRIT", true)
}

void logError(const char *format, ...)
{
	_DO_LOG((&g_log_context), LOG_ERR, "ERROR", false)
}

void logWarning(const char *format, ...)
{
	_DO_LOG((&g_log_context), LOG_WARNING, "WARNING", false)
}

void logNotice(const char *format, ...)
{
	_DO_LOG((&g_log_context), LOG_NOTICE, "NOTICE", false)
}

void logInfo(const char *format, ...)
{
	_DO_LOG((&g_log_context), LOG_INFO, "INFO", true)
}

void logDebug(const char *format, ...)
{
	_DO_LOG((&g_log_context), LOG_DEBUG, "DEBUG", true)
}

#endif

