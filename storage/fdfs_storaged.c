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
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "fastcommon/shared_func.h"
#include "fastcommon/pthread_func.h"
#include "fastcommon/process_ctrl.h"
#include "fastcommon/logger.h"
#include "fdfs_global.h"
#include "fastcommon/ini_file_reader.h"
#include "fastcommon/sockopt.h"
#include "sf/sf_service.h"
#include "sf/sf_util.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client_thread.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_sync.h"
#include "storage_service.h"
#include "fastcommon/sched_thread.h"
#include "storage_dio.h"
#include "trunk_mem.h"
#include "trunk_sync.h"
#include "trunk_shared.h"
#include "file_id_hashtable.h"

#ifdef WITH_HTTPD
#include "storage_httpd.h"
#endif

#if defined(DEBUG_FLAG) 
#include "storage_dump.h"
#endif

#define ACCEPT_STAGE_NONE    0
#define ACCEPT_STAGE_DOING   1
#define ACCEPT_STAGE_DONE    2

static bool daemon_mode = true;
static bool bTerminateFlag = false;
static char accept_stage = ACCEPT_STAGE_NONE;

static void sigQuitHandler(int sig);
static void sigHupHandler(int sig);
static void sigUsrHandler(int sig);
static void sigAlarmHandler(int sig);

static int setup_schedule_tasks();
static int setupSignalHandlers();

#if defined(DEBUG_FLAG)

/*
#if defined(OS_LINUX)
static void sigSegvHandler(int signum, siginfo_t *info, void *ptr);
#endif
*/

static void sigDumpHandler(int sig);
#endif

int main(int argc, char *argv[])
{
	const char *conf_filename;
    char *action;
	int result;
	int wait_count;
	pthread_t schedule_tid;
	char pidFilename[MAX_PATH_SIZE];
	bool stop;

	if (argc < 2)
	{
        sf_usage(argv[0]);
		return 1;
	}

    conf_filename = sf_parse_daemon_mode_and_action(argc, argv,
            &g_fdfs_version, &daemon_mode, &action);
    if (conf_filename == NULL)
    {
        return 0;
    }

	g_current_time = time(NULL);
	log_init2();
	if ((result=trunk_shared_init()) != 0)
    {
		log_destroy();
		return result;
    }

	if ((result=sf_get_base_path_from_conf_file(conf_filename)) != 0)
	{
		log_destroy();
		return result;
	}

    if ((result=storage_check_and_make_global_data_path()) != 0)
    {
		log_destroy();
        return result;
    }

	snprintf(pidFilename, sizeof(pidFilename),
		"%s/data/fdfs_storaged.pid", SF_G_BASE_PATH_STR);
	if ((result=process_action(pidFilename, action, &stop)) != 0)
	{
		if (result == EINVAL)
		{
			sf_usage(argv[0]);
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

    if (daemon_mode) {
        daemon_init(false);
    }
	umask(0);

    if ((result=setupSignalHandlers()) != 0)
    {
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
    }

	if ((result=storage_func_init(conf_filename)) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

    if ((result=sf_socket_server()) != 0)
    {
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
		SF_G_CONTINUE_FLAG = false;
		return result;
	}

	if ((result=tracker_report_init()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"tracker_report_init fail, program exit!", __LINE__);
		SF_G_CONTINUE_FLAG = false;
		return result;
	}

	if ((result=storage_service_init()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"storage_service_init fail, program exit!", __LINE__);
		SF_G_CONTINUE_FLAG = false;
		return result;
	}

	if ((result=set_rand_seed()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"set_rand_seed fail, program exit!", __LINE__);
		SF_G_CONTINUE_FLAG = false;
		return result;
	}


#ifdef WITH_HTTPD
	if (!g_http_params.disabled)
	{
		if ((result=storage_httpd_start(SF_G_INNER_BIND_ADDR)) != 0)
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
		SF_G_CONTINUE_FLAG = false;
		storage_func_destroy();
		log_destroy();
		return result;
	}

    if ((result=sf_startup_schedule(&schedule_tid)) != 0)
    {
        log_destroy();
        return result;
    }

    if ((result=setup_schedule_tasks()) != 0)
    {
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
    }

    if ((result=file_id_hashtable_init()) != 0)
    {
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
    }

	if ((result=set_run_by(g_sf_global_vars.run_by.group,
                    g_sf_global_vars.run_by.user)) != 0)
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
	accept_stage = ACCEPT_STAGE_DOING;
	
    sf_accept_loop();
	accept_stage = ACCEPT_STAGE_DONE;

	fdfs_binlog_sync_func(NULL);  //binlog fsync

	if (g_schedule_flag)
	{
		pthread_kill(schedule_tid, SIGINT);
	}

	storage_dio_terminate();

	kill_tracker_report_threads();
	kill_storage_sync_threads();

	wait_count = 0;
	while (SF_G_ALIVE_THREAD_COUNT != 0 || g_dio_thread_count != 0 ||
		g_tracker_reporter_count > 0 || g_schedule_flag)
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
		if (++wait_count > 9000)
		{
			logWarning("waiting timeout, exit!");
			break;
		}
	}

	tracker_report_destroy();
	storage_service_destroy();
	storage_sync_destroy();

	if (g_if_use_trunk_file)
	{
		trunk_sync_destroy();
		storage_trunk_destroy();
	}

	storage_func_destroy();
	delete_pid_file(pidFilename);
	logInfo("exit normally.\n");
	log_destroy();
	
	return 0;
}

static void sigQuitHandler(int sig)
{
	if (!bTerminateFlag)
	{
        tcp_set_try_again_when_interrupt(false);
		set_timer(1, 1, sigAlarmHandler);

		bTerminateFlag = true;
		SF_G_CONTINUE_FLAG = false;

		logCrit("file: "__FILE__", line: %d, " \
			"catch signal %d, program exiting...", \
			__LINE__, sig);
	}
}

static void sigAlarmHandler(int sig)
{
	ConnectionInfo server;

	if (accept_stage != ACCEPT_STAGE_DOING)
	{
		return;
	}

	logDebug("file: "__FILE__", line: %d, " \
		"signal server to quit...", __LINE__);

    if (SF_G_IPV4_ENABLED)
    {
        server.af = AF_INET;
        if (*SF_G_INNER_BIND_ADDR4 != '\0')
        {
            strcpy(server.ip_addr, SF_G_INNER_BIND_ADDR4);
        }
        else
        {
            strcpy(server.ip_addr, LOCAL_LOOPBACK_IPv4);
        }
    }
    else
    {
        server.af = AF_INET6;
        if (*SF_G_INNER_BIND_ADDR6 != '\0')
        {
            strcpy(server.ip_addr, SF_G_INNER_BIND_ADDR6);
        }
        else
        {
            strcpy(server.ip_addr, LOCAL_LOOPBACK_IPv6);
        }
    }
	server.port = SF_G_INNER_PORT;
	server.sock = -1;

	if (conn_pool_connect_server(&server, SF_G_CONNECT_TIMEOUT * 1000) != 0)
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
	if (g_sf_global_vars.error_log.rotate_everyday)
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
		"%s/logs/storage_dump.log", SF_G_BASE_PATH_STR);
	fdfs_dump_storage_global_vars_to_file(filename);

	bDumpFlag = false;
}
#endif

static int setup_schedule_tasks()
{
#define SCHEDULE_ENTRIES_MAX_COUNT 8

	ScheduleEntry scheduleEntries[SCHEDULE_ENTRIES_MAX_COUNT];
	ScheduleArray scheduleArray;

	scheduleArray.entries = scheduleEntries;
	scheduleArray.count = 0;
	memset(scheduleEntries, 0, sizeof(scheduleEntries));

	INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
		sched_generate_next_id(), TIME_NONE, TIME_NONE, TIME_NONE,
		g_sync_binlog_buff_interval, fdfs_binlog_sync_func, NULL);
	scheduleArray.count++;

	INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
		sched_generate_next_id(), TIME_NONE, TIME_NONE, TIME_NONE,
		g_sync_stat_file_interval, fdfs_stat_file_sync_func, NULL);
	scheduleArray.count++;

	if (g_if_use_trunk_file)
	{
		INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
			sched_generate_next_id(), TIME_NONE, TIME_NONE, TIME_NONE,
			1, trunk_binlog_sync_func, NULL);
		scheduleArray.count++;
	}

	if (g_use_access_log)
	{
		INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
			sched_generate_next_id(), TIME_NONE, TIME_NONE, TIME_NONE,
			g_sf_global_vars.error_log.sync_log_buff_interval,
            log_sync_func, &g_access_log_context);
		scheduleArray.count++;

		if (g_rotate_access_log)
		{
			INIT_SCHEDULE_ENTRY_EX(scheduleEntries[scheduleArray.count],
				sched_generate_next_id(), g_access_log_rotate_time,
				24 * 3600, log_notify_rotate, &g_access_log_context);
			scheduleArray.count++;

			if (g_sf_global_vars.error_log.keep_days > 0)
			{
				log_set_keep_days(&g_access_log_context,
					g_sf_global_vars.error_log.keep_days);

				INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
					sched_generate_next_id(), 1, 0, 0, 24 * 3600,
					log_delete_old_files, &g_access_log_context);
				scheduleArray.count++;
			}
		}
	}

	if (g_compress_binlog)
	{
		INIT_SCHEDULE_ENTRY_EX1(scheduleEntries[scheduleArray.count],
			sched_generate_next_id(), g_compress_binlog_time,
			24 * 3600, fdfs_binlog_compress_func, NULL, true);
		scheduleArray.count++;
    }

    return sched_add_entries(&scheduleArray);
}

static int setupSignalHandlers()
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);

	act.sa_handler = sigUsrHandler;
	if(sigaction(SIGUSR1, &act, NULL) < 0 || \
		sigaction(SIGUSR2, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		return errno != 0 ? errno : EFAULT;
	}

	act.sa_handler = sigHupHandler;
	if(sigaction(SIGHUP, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		return errno != 0 ? errno : EFAULT;
	}
	
	act.sa_handler = SIG_IGN;
	if(sigaction(SIGPIPE, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		return errno != 0 ? errno : EFAULT;
	}

	act.sa_handler = sigQuitHandler;
	if(sigaction(SIGINT, &act, NULL) < 0 || \
		sigaction(SIGTERM, &act, NULL) < 0 || \
		sigaction(SIGQUIT, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		return errno != 0 ? errno : EFAULT;
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
		return errno != 0 ? errno : EFAULT;
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
		return errno != 0 ? errno : EFAULT;
	}
#endif

    return 0;
}
