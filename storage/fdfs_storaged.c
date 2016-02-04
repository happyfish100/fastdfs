/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
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
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "shared_func.h"
#include "pthread_func.h"
#include "process_ctrl.h"
#include "logger.h"
#include "fdfs_global.h"
#include "ini_file_reader.h"
#include "sockopt.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client_thread.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_sync.h"
#include "storage_service.h"
#include "sched_thread.h"
#include "storage_dio.h"
#include "trunk_mem.h"
#include "trunk_sync.h"
#include "trunk_shared.h"

#ifdef WITH_HTTPD
#include "storage_httpd.h"
#endif

#if defined(DEBUG_FLAG) 
#include "storage_dump.h"
#endif

static bool bTerminateFlag = false;
static bool bAcceptEndFlag = false;

static void sigQuitHandler(int sig);
static void sigHupHandler(int sig);
static void sigUsrHandler(int sig);
static void sigAlarmHandler(int sig);

#if defined(DEBUG_FLAG)

/*
#if defined(OS_LINUX)
static void sigSegvHandler(int signum, siginfo_t *info, void *ptr);
#endif
*/

static void sigDumpHandler(int sig);
#endif

#define SCHEDULE_ENTRIES_MAX_COUNT 9

static void usage(const char *program)
{
	fprintf(stderr, "Usage: %s <config_file> [start | stop | restart]\n",
		program);
}

int main(int argc, char *argv[])
{
	char *conf_filename;
	int result;
	int sock;
	int wait_count;
	pthread_t schedule_tid;
	struct sigaction act;
	ScheduleEntry scheduleEntries[SCHEDULE_ENTRIES_MAX_COUNT];
	ScheduleArray scheduleArray;
	char pidFilename[MAX_PATH_SIZE];
	bool stop;

	if (argc < 2)
	{
		usage(argv[0]);
		return 1;
	}

	g_current_time = time(NULL);
	g_up_time = g_current_time;

	log_init2();
	trunk_shared_init();

	conf_filename = argv[1];
	if ((result=get_base_path_from_conf_file(conf_filename,
		g_fdfs_base_path, sizeof(g_fdfs_base_path))) != 0)
	{
		log_destroy();
		return result;
	}

	snprintf(pidFilename, sizeof(pidFilename),
		"%s/data/fdfs_storaged.pid", g_fdfs_base_path);
	if ((result=process_action(pidFilename, argv[2], &stop)) != 0)
	{
		if (result == EINVAL)
		{
			usage(argv[0]);
		}
		log_destroy();
		return result;
	}
	if (stop)
	{
		log_destroy();
		return 0;
	}

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
	if (getExeAbsoluteFilename(argv[0], g_exe_name, \
		sizeof(g_exe_name)) == NULL)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return errno != 0 ? errno : ENOENT;
	}
#endif

	daemon_init(false);
	umask(0);

	memset(g_bind_addr, 0, sizeof(g_bind_addr));
	if ((result=storage_func_init(conf_filename, \
			g_bind_addr, sizeof(g_bind_addr))) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	sock = socketServer(g_bind_addr, g_server_port, &result);
	if (sock < 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	if ((result=tcpsetserveropt(sock, g_fdfs_network_timeout)) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	if ((result=write_to_pid_file(pidFilename)) != 0)
	{
		log_destroy();
		return result;
	}

	if ((result=storage_sync_init()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"storage_sync_init fail, program exit!", __LINE__);
		g_continue_flag = false;
		return result;
	}

	if ((result=tracker_report_init()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"tracker_report_init fail, program exit!", __LINE__);
		g_continue_flag = false;
		return result;
	}

	if ((result=storage_service_init()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"storage_service_init fail, program exit!", __LINE__);
		g_continue_flag = false;
		return result;
	}

	if ((result=set_rand_seed()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"set_rand_seed fail, program exit!", __LINE__);
		g_continue_flag = false;
		return result;
	}

	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);

	act.sa_handler = sigUsrHandler;
	if(sigaction(SIGUSR1, &act, NULL) < 0 || \
		sigaction(SIGUSR2, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		logCrit("exit abnormally!\n");
		return errno;
	}

	act.sa_handler = sigHupHandler;
	if(sigaction(SIGHUP, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		logCrit("exit abnormally!\n");
		return errno;
	}
	
	act.sa_handler = SIG_IGN;
	if(sigaction(SIGPIPE, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		logCrit("exit abnormally!\n");
		return errno;
	}

	act.sa_handler = sigQuitHandler;
	if(sigaction(SIGINT, &act, NULL) < 0 || \
		sigaction(SIGTERM, &act, NULL) < 0 || \
		sigaction(SIGQUIT, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		logCrit("exit abnormally!\n");
		return errno;
	}

#if defined(DEBUG_FLAG)

/*
#if defined(OS_LINUX)
	memset(&act, 0, sizeof(act));
        act.sa_sigaction = sigSegvHandler;
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGSEGV, &act, NULL) < 0 || \
        	sigaction(SIGABRT, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		logCrit("exit abnormally!\n");
		return errno;
	}
#endif
*/

	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_handler = sigDumpHandler;
	if(sigaction(SIGUSR1, &act, NULL) < 0 || \
		sigaction(SIGUSR2, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		logCrit("exit abnormally!\n");
		return errno;
	}
#endif

#ifdef WITH_HTTPD
	if (!g_http_params.disabled)
	{
		if ((result=storage_httpd_start(g_bind_addr)) != 0)
		{
			logCrit("file: "__FILE__", line: %d, " \
				"storage_httpd_start fail, " \
				"program exit!", __LINE__);
			return result;
		}
	}
#endif

	if ((result=tracker_report_thread_start()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"tracker_report_thread_start fail, " \
			"program exit!", __LINE__);
		g_continue_flag = false;
		storage_func_destroy();
		log_destroy();
		return result;
	}

	scheduleArray.entries = scheduleEntries;
	scheduleArray.count = 0;
	memset(scheduleEntries, 0, sizeof(scheduleEntries));

	INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
		scheduleArray.count + 1, TIME_NONE, TIME_NONE, TIME_NONE,
		g_sync_log_buff_interval, log_sync_func, &g_log_context);
	scheduleArray.count++;

	INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
		scheduleArray.count + 1, TIME_NONE, TIME_NONE, TIME_NONE,
		g_sync_binlog_buff_interval, fdfs_binlog_sync_func, NULL);
	scheduleArray.count++;

	INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
		scheduleArray.count + 1, TIME_NONE, TIME_NONE, TIME_NONE,
		g_sync_stat_file_interval, fdfs_stat_file_sync_func, NULL);
	scheduleArray.count++;

	if (g_if_use_trunk_file)
	{
		INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
			scheduleArray.count + 1, TIME_NONE, TIME_NONE, TIME_NONE,
			1, trunk_binlog_sync_func, NULL);
		scheduleArray.count++;
	}

	if (g_use_access_log)
	{
		INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
			scheduleArray.count + 1, TIME_NONE, TIME_NONE, TIME_NONE,
			g_sync_log_buff_interval, log_sync_func, &g_access_log_context);
		scheduleArray.count++;

		if (g_rotate_access_log)
		{
			INIT_SCHEDULE_ENTRY_EX(scheduleEntries[scheduleArray.count],
				scheduleArray.count + 1, g_access_log_rotate_time,
				24 * 3600, log_notify_rotate, &g_access_log_context);
			scheduleArray.count++;

			if (g_log_file_keep_days > 0)
			{
				log_set_keep_days(&g_access_log_context,
					g_log_file_keep_days);

				INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
					scheduleArray.count + 1, 1, 0, 0, 24 * 3600,
					log_delete_old_files, &g_access_log_context);
				scheduleArray.count++;
			}
		}
	}

	if (g_rotate_error_log)
	{
		INIT_SCHEDULE_ENTRY_EX(scheduleEntries[scheduleArray.count],
			scheduleArray.count + 1, g_error_log_rotate_time,
			24 * 3600, log_notify_rotate, &g_log_context);
		scheduleArray.count++;

		if (g_log_file_keep_days > 0)
		{
			log_set_keep_days(&g_log_context, g_log_file_keep_days);

			INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
				scheduleArray.count + 1, 1, 0, 0, 24 * 3600,
				log_delete_old_files, &g_log_context);
			scheduleArray.count++;
		}
	}

	if ((result=sched_start(&scheduleArray, &schedule_tid, \
		g_thread_stack_size, (bool * volatile)&g_continue_flag)) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	if ((result=set_run_by(g_run_by_group, g_run_by_user)) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	if ((result=storage_dio_init()) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}
	log_set_cache(true);

	bTerminateFlag = false;
	bAcceptEndFlag = false;
	
	storage_accept_loop(sock);
	bAcceptEndFlag = true;

	fdfs_binlog_sync_func(NULL);  //binlog fsync

	if (g_schedule_flag)
	{
		pthread_kill(schedule_tid, SIGINT);
	}

	storage_terminate_threads();
	storage_dio_terminate();

	kill_tracker_report_threads();
	kill_storage_sync_threads();

	wait_count = 0;
	while (g_storage_thread_count != 0 || \
		g_dio_thread_count != 0 || \
		g_tracker_reporter_count > 0 || \
		g_schedule_flag)
	{
/*
#if defined(DEBUG_FLAG) && defined(OS_LINUX)
		if (bSegmentFault)
		{
			sleep(5);
			break;
		}
#endif
*/

		usleep(10000);
		if (++wait_count > 6000)
		{
			logWarning("waiting timeout, exit!");
			break;
		}
	}

	tracker_report_destroy();
	storage_service_destroy();
	storage_sync_destroy();
	storage_func_destroy();

	if (g_if_use_trunk_file)
	{
		trunk_sync_destroy();
		storage_trunk_destroy();
	}

	logInfo("exit normally.\n");
	log_destroy();
	
	delete_pid_file(pidFilename);
	return 0;
}

static void sigQuitHandler(int sig)
{
	if (!bTerminateFlag)
	{
		set_timer(1, 1, sigAlarmHandler);

		bTerminateFlag = true;
		g_continue_flag = false;

		logCrit("file: "__FILE__", line: %d, " \
			"catch signal %d, program exiting...", \
			__LINE__, sig);
	}
}

static void sigAlarmHandler(int sig)
{
	ConnectionInfo server;

	if (bAcceptEndFlag)
	{
		return;
	}

	logDebug("file: "__FILE__", line: %d, " \
		"signal server to quit...", __LINE__);

	if (*g_bind_addr != '\0')
	{
		strcpy(server.ip_addr, g_bind_addr);
	}
	else
	{
		strcpy(server.ip_addr, "127.0.0.1");
	}
	server.port = g_server_port;
	server.sock = -1;

	if (conn_pool_connect_server(&server, g_fdfs_connect_timeout) != 0)
	{
		return;
	}

	fdfs_quit(&server);
	conn_pool_disconnect_server(&server);

	logDebug("file: "__FILE__", line: %d, " \
		"signal server to quit done", __LINE__);
}

static void sigHupHandler(int sig)
{
	if (g_rotate_error_log)
	{
		g_log_context.rotate_immediately = true;
	}

	if (g_rotate_access_log)
	{
		g_access_log_context.rotate_immediately = true;
	}

	logInfo("file: "__FILE__", line: %d, " \
		"catch signal %d, rotate log", __LINE__, sig);
}

static void sigUsrHandler(int sig)
{
	logInfo("file: "__FILE__", line: %d, " \
		"catch signal %d, ignore it", __LINE__, sig);
}

#if defined(DEBUG_FLAG)
static void sigDumpHandler(int sig)
{
	static bool bDumpFlag = false;
	char filename[MAX_PATH_SIZE];

	if (bDumpFlag)
	{
		return;
	}

	bDumpFlag = true;

	snprintf(filename, sizeof(filename), 
		"%s/logs/storage_dump.log", g_fdfs_base_path);
	fdfs_dump_storage_global_vars_to_file(filename);

	bDumpFlag = false;
}
#endif

