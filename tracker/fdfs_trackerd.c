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
#include "fastcommon/process_ctrl.h"
#include "fastcommon/logger.h"
#include "fdfs_global.h"
#include "fastcommon/base64.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/sched_thread.h"
#include "sf/sf_service.h"
#include "sf/sf_util.h"
#include "tracker_types.h"
#include "tracker_mem.h"
#include "tracker_service.h"
#include "tracker_global.h"
#include "tracker_proto.h"
#include "tracker_func.h"
#include "tracker_status.h"
#include "tracker_relationship.h"

#if defined(DEBUG_FLAG)
#include "tracker_dump.h"
#endif

static bool daemon_mode = true;
static bool bTerminateFlag = false;
static bool bAcceptEndFlag = false;

static void sigQuitHandler(int sig);
static void sigHupHandler(int sig);
static void sigUsrHandler(int sig);
static void sigAlarmHandler(int sig);

#if defined(DEBUG_FLAG)
static void sigDumpHandler(int sig);
#endif

static int setup_signal_handlers()
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
		return errno;
	}

	act.sa_handler = sigHupHandler;
	if(sigaction(SIGHUP, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		return errno;
	}

	act.sa_handler = SIG_IGN;
	if(sigaction(SIGPIPE, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
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
		return errno;
	}

#if defined(DEBUG_FLAG)
	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_handler = sigDumpHandler;
	if(sigaction(SIGUSR1, &act, NULL) < 0 || \
		sigaction(SIGUSR2, &act, NULL) < 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"call sigaction fail, errno: %d, error info: %s", \
			__LINE__, errno, STRERROR(errno));
		return errno;
	}
#endif

    return 0;
}

static int setup_schedule_tasks()
{
#define SCHEDULE_ENTRIES_COUNT 4
    ScheduleEntry scheduleEntries[SCHEDULE_ENTRIES_COUNT];
    ScheduleArray scheduleArray;

    scheduleArray.entries = scheduleEntries;
    scheduleArray.count = 0;
    memset(scheduleEntries, 0, sizeof(scheduleEntries));

    INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
            sched_generate_next_id(), TIME_NONE, TIME_NONE, TIME_NONE,
            g_check_active_interval, tracker_mem_check_alive, NULL);
    scheduleArray.count++;

    INIT_SCHEDULE_ENTRY(scheduleEntries[scheduleArray.count],
            sched_generate_next_id(), 0, 0, 0,
            TRACKER_SYNC_STATUS_FILE_INTERVAL,
            tracker_write_status_to_file, NULL);
    scheduleArray.count++;

    return sched_add_entries(&scheduleArray);
}

int main(int argc, char *argv[])
{
#define PID_FILENAME_STR  "data/fdfs_trackerd.pid"
#define PID_FILENAME_LEN  (sizeof(PID_FILENAME_STR) - 1)

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

	if ((result=sf_get_base_path_from_conf_file(conf_filename)) != 0)
	{
		log_destroy();
		return result;
	}

    fc_get_full_filename(SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN,
            PID_FILENAME_STR, PID_FILENAME_LEN, pidFilename);
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

	if ((result=tracker_load_from_conf_file(conf_filename)) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	if ((result=tracker_load_status_from_file(&g_tracker_last_status)) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	base64_init_ex(&g_fdfs_base64_context, 0, '-', '_', '.');
	if ((result=set_rand_seed()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"set_rand_seed fail, program exit!", __LINE__);
		return result;
	}

	if ((result=tracker_mem_init()) != 0)
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

    if (daemon_mode)
    {
        daemon_init(false);
    }
	umask(0);
	
	if ((result=write_to_pid_file(pidFilename)) != 0)
	{
		log_destroy();
		return result;
	}

	if ((result=tracker_service_init()) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	if ((result=setup_signal_handlers()) != 0)
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

    if ((result=sf_startup_schedule(&schedule_tid)) != 0)
    {
        log_destroy();
        return result;
    }

    if ((result=setup_schedule_tasks()) != 0)
    {
        log_destroy();
        return result;
    }

	if ((result=tracker_relationship_init()) != 0)
	{
		logCrit("exit abnormally!\n");
		log_destroy();
		return result;
	}

	log_set_cache(true);

	bTerminateFlag = false;
	bAcceptEndFlag = false;

    sf_accept_loop();

	bAcceptEndFlag = true;
	if (g_schedule_flag)
	{
		pthread_kill(schedule_tid, SIGINT);
	}

	wait_count = 0;
	while ((SF_G_ALIVE_THREAD_COUNT != 0) || g_schedule_flag)
	{
		fc_sleep_ms(10);
		if (++wait_count > 3000)
		{
			logWarning("waiting timeout, exit!");
			break;
		}
	}
	
	tracker_mem_destroy();
	tracker_service_destroy();
	tracker_relationship_destroy();
	
	logInfo("exit normally.\n");
	log_destroy();
	
	delete_pid_file(pidFilename);
	return 0;
}

#if defined(DEBUG_FLAG)
static void sigDumpHandler(int sig)
{
#define DUMP_FILENAME_STR  "logs/tracker_dump.log"
#define DUMP_FILENAME_LEN  (sizeof(DUMP_FILENAME_STR) - 1)

	static bool bDumpFlag = false;
	char filename[256];

	if (bDumpFlag)
	{
		return;
	}

	bDumpFlag = true;

    fc_get_full_filename(SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN,
            DUMP_FILENAME_STR, DUMP_FILENAME_LEN, filename);
	fdfs_dump_tracker_global_vars_to_file(filename);

	bDumpFlag = false;
}

#endif

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

static void sigHupHandler(int sig)
{
	if (g_sf_global_vars.error_log.rotate_everyday)
	{
		g_log_context.rotate_immediately = true;
	}

	logInfo("file: "__FILE__", line: %d, "
		"catch signal %d, rotate log", __LINE__, sig);
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

    memset(&server, 0, sizeof(server));
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

static void sigUsrHandler(int sig)
{
	logInfo("file: "__FILE__", line: %d, " \
		"catch signal %d, ignore it", __LINE__, sig);
}

