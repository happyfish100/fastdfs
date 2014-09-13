/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "storage_dump.h"
#include "shared_func.h"
#include "sched_thread.h"
#include "logger.h"
#include "hash.h"
#include "connection_pool.h"
#include "fdfs_global.h"
#include "storage_global.h"
#include "storage_service.h"
#include "storage_sync.h"
#include "trunk_mem.h"
#include "trunk_sync.h"

static int fdfs_dump_global_vars(char *buff, const int buffSize)
{
	char szStorageJoinTime[32];
	char szSyncUntilTimestamp[32];
	char szUptime[32];
	char reserved_space_str[32];
	int total_len;
	int i;

	total_len = snprintf(buff, buffSize,
		"g_fdfs_connect_timeout=%ds\n"
		"g_fdfs_network_timeout=%ds\n"
		"g_fdfs_base_path=%s\n"
		"g_fdfs_version=%d.%02d\n"
		"g_continue_flag=%d\n"
		"g_schedule_flag=%d\n"
		"g_server_port=%d\n"
		"g_max_connections=%d\n"
		"g_storage_thread_count=%d\n"
		"g_group_name=%s\n"
		"g_sync_log_buff_interval=%d\n"
		"g_subdir_count_per_path=%d\n"
		"g_http_port=%d\n"
		"g_last_server_port=%d\n"
		"g_last_http_port=%d\n"
		"g_allow_ip_count=%d\n"
		"g_run_by_group=%s\n"
		"g_run_by_user=%s\n"
		"g_http_domain=%s\n"
		"g_file_distribute_path_mode=%d\n"
		"g_file_distribute_rotate_count=%d\n"
		"g_fsync_after_written_bytes=%d\n"
		"g_dist_path_index_high=%d\n"
		"g_dist_path_index_low=%d\n"
		"g_dist_write_file_count=%d\n"
		"g_disk_rw_direct=%d\n"
		"g_disk_rw_separated=%d\n"
		"g_disk_reader_threads=%d\n"
		"g_disk_writer_threads=%d\n"
		"g_extra_open_file_flags=%d\n"
		"g_tracker_reporter_count=%d\n"
		"g_heart_beat_interval=%d\n"
		"g_stat_report_interval=%d\n"
		"g_sync_wait_usec=%dms\n"
		"g_sync_interval=%dms\n"
		"g_sync_start_time=%d:%d\n"
		"g_sync_end_time=%d:%d\n"
		"g_sync_part_time=%d\n"
		"g_sync_log_buff_interval=%ds\n"
		"g_sync_binlog_buff_interval=%ds\n"
		"g_write_mark_file_freq=%d\n"
		"g_sync_stat_file_interval=%ds\n"
		"g_storage_join_time=%s\n"
		"g_sync_old_done=%d\n"
		"g_sync_src_id=%s\n"
		"g_sync_until_timestamp=%s\n"
		"g_my_server_id_str=%s\n"
		"g_tracker_client_ip=%s\n"
		"g_last_storage_ip=%s\n"
		"g_check_file_duplicate=%d\n"
		"g_key_namespace=%s\n"
		"g_namespace_len=%d\n"
		"g_bind_addr=%s\n"
		"g_client_bind_addr=%d\n"
		"g_storage_ip_changed_auto_adjust=%d\n"
		"g_thread_kill_done=%d\n"
		"g_thread_stack_size=%d\n"
		"g_upload_priority=%d\n"
		"g_up_time=%s\n"
		"g_if_alias_prefix=%s\n"
		"g_binlog_fd=%d\n"
		"g_binlog_index=%d\n"
		"g_storage_sync_thread_count=%d\n"
		"g_use_storage_id=%d\n"
		"g_if_use_trunk_file=%d\n"
		"g_if_trunker_self=%d\n"
		"g_slot_min_size=%d\n"
		"g_trunk_file_size=%d\n"
		"g_store_path_mode=%d\n"
		"storage_reserved_mb=%s\n"
		"g_avg_storage_reserved_mb=%d\n"
		"g_store_path_index=%d\n"
		"g_current_trunk_file_id=%d\n"
		"g_trunk_sync_thread_count=%d\n"
		"g_trunk_server=%s:%d\n"
		"g_trunk_total_free_space=%"PRId64"\n"
		"g_use_connection_pool=%d\n"
		"g_connection_pool_max_idle_time=%d\n"
		"connection_pool_conn_count=%d\n"
	#ifdef WITH_HTTPD
		"g_http_params.disabled=%d\n"
		"g_http_params.anti_steal_token=%d\n"
		"g_http_params.server_port=%d\n"
		"g_http_params.content_type_hash item count=%d\n"
		"g_http_params.anti_steal_secret_key length=%d\n"
		"g_http_params.token_check_fail_buff length=%d\n"
		"g_http_params.default_content_type=%s\n"
		"g_http_params.token_check_fail_content_type=%s\n"
		"g_http_params.token_ttl=%d\n"
		"g_http_trunk_size=%d\n"
	#endif
	#if defined(DEBUG_FLAG) && defined(OS_LINUX)
		"g_exe_name=%s\n"
	#endif
		, g_fdfs_connect_timeout
		, g_fdfs_network_timeout
		, g_fdfs_base_path
		, g_fdfs_version.major, g_fdfs_version.minor
		, g_continue_flag
		, g_schedule_flag
		, g_server_port
		, g_max_connections
		, g_storage_thread_count
		, g_group_name
		, g_sync_log_buff_interval
		, g_subdir_count_per_path 
		, g_http_port 
		, g_last_server_port 
		, g_last_http_port
		, g_allow_ip_count
		, g_run_by_group
		, g_run_by_user
		, g_http_domain
		, g_file_distribute_path_mode
		, g_file_distribute_rotate_count
		, g_fsync_after_written_bytes
		, g_dist_path_index_high
		, g_dist_path_index_low
		, g_dist_write_file_count
		, g_disk_rw_direct
		, g_disk_rw_separated
		, g_disk_reader_threads
		, g_disk_writer_threads
		, g_extra_open_file_flags
		, g_tracker_reporter_count
		, g_heart_beat_interval
		, g_stat_report_interval
		, g_sync_wait_usec / 1000
		, g_sync_interval
		, g_sync_start_time.hour, g_sync_start_time.minute
		, g_sync_end_time.hour, g_sync_end_time.minute
		, g_sync_part_time
		, g_sync_log_buff_interval
		, g_sync_binlog_buff_interval
		, g_write_mark_file_freq
		, g_sync_stat_file_interval
		, formatDatetime(g_storage_join_time, "%Y-%m-%d %H:%M:%S", 
			szStorageJoinTime, sizeof(szStorageJoinTime))
		, g_sync_old_done
		, g_sync_src_id
		, formatDatetime(g_sync_until_timestamp, "%Y-%m-%d %H:%M:%S", 
			szSyncUntilTimestamp, sizeof(szSyncUntilTimestamp))
                , g_my_server_id_str
		, g_tracker_client_ip
		, g_last_storage_ip
		, g_check_file_duplicate
		, g_key_namespace
		, g_namespace_len
		, g_bind_addr
		, g_client_bind_addr
		, g_storage_ip_changed_auto_adjust
		, g_thread_kill_done
		, g_thread_stack_size
		, g_upload_priority
		, formatDatetime(g_up_time, "%Y-%m-%d %H:%M:%S", 
			szUptime, sizeof(szUptime))
		, g_if_alias_prefix
		, g_binlog_fd
		, g_binlog_index
		, g_storage_sync_thread_count
		, g_use_storage_id
		, g_if_use_trunk_file
		, g_if_trunker_self
		, g_slot_min_size
		, g_trunk_file_size
		, g_store_path_mode
		, fdfs_storage_reserved_space_to_string( \
			&g_storage_reserved_space, reserved_space_str) \
		, g_avg_storage_reserved_mb
		, g_store_path_index
		, g_current_trunk_file_id
		, g_trunk_sync_thread_count
		, g_trunk_server.ip_addr, g_trunk_server.port
		, g_trunk_total_free_space
		, g_use_connection_pool
		, g_connection_pool_max_idle_time
		, g_use_connection_pool ? conn_pool_get_connection_count( \
			&g_connection_pool) : 0
	#ifdef WITH_HTTPD
		, g_http_params.disabled
		, g_http_params.anti_steal_token
		, g_http_params.server_port
		, hash_count(&(g_http_params.content_type_hash))
		, g_http_params.anti_steal_secret_key.length
		, g_http_params.token_check_fail_buff.length
		, g_http_params.default_content_type
		, g_http_params.token_check_fail_content_type
		, g_http_params.token_ttl
		, g_http_trunk_size
	#endif
		
	#if defined(DEBUG_FLAG) && defined(OS_LINUX)
		, g_exe_name
	#endif
	);

	total_len += snprintf(buff + total_len, buffSize - total_len,
			"\ng_fdfs_store_paths.count=%d\n", g_fdfs_store_paths.count);
	for (i=0; i<g_fdfs_store_paths.count; i++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len,
				"\tg_fdfs_store_paths.paths[%d]=%s, " \
				"total=%d MB, free=%d MB\n", i, \
				g_fdfs_store_paths.paths[i], \
				g_path_space_list[i].total_mb, \
				g_path_space_list[i].free_mb);
	}

	total_len += snprintf(buff + total_len, buffSize - total_len,
			"\ng_local_host_ip_count=%d\n", g_local_host_ip_count);
	for (i=0; i<g_local_host_ip_count; i++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len,
				"\tg_local_host_ip_addrs[%d]=%s\n", i, 
				g_local_host_ip_addrs + i * IP_ADDRESS_SIZE);
	}

	return total_len;
}

static int fdfs_dump_tracker_servers(char *buff, const int buffSize)
{
	int total_len;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pTrackerEnd;

	total_len = snprintf(buff, buffSize, \
		"\ng_tracker_group.server_count=%d, " \
		"g_tracker_group.leader_index=%d\n", \
		g_tracker_group.server_count, \
		g_tracker_group.leader_index);
	if (g_tracker_group.server_count == 0)
	{
		return total_len;
	}

	pTrackerEnd = g_tracker_group.servers + g_tracker_group.server_count;
	for (pTrackerServer=g_tracker_group.servers; \
		pTrackerServer<pTrackerEnd; pTrackerServer++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len,
			"\t%d. tracker server=%s:%d\n", \
			(int)(pTrackerServer - g_tracker_group.servers) + 1, \
			pTrackerServer->ip_addr, pTrackerServer->port);
	}

	return total_len;
}

static int fdfs_dump_storage_servers(char *buff, const int buffSize)
{
	int total_len;
	char szLastSyncSrcTimestamp[32];
	FDFSStorageServer *pServer;
	FDFSStorageServer *pServerEnd;
	FDFSStorageServer **ppServer;
	FDFSStorageServer **ppServerEnd;

	total_len = snprintf(buff, buffSize,
			"\ng_storage_count=%d\n", g_storage_count);
	pServerEnd = g_storage_servers + g_storage_count;
	for (pServer=g_storage_servers; pServer<pServerEnd; pServer++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len,
			"\t%d. server: %s, status: %d, sync timestamp: %s\n", 
			(int)(pServer - g_storage_servers) + 1, 
			pServer->server.ip_addr, pServer->server.status,
			formatDatetime(pServer->last_sync_src_timestamp, 
				"%Y-%m-%d %H:%M:%S", szLastSyncSrcTimestamp, 
				sizeof(szLastSyncSrcTimestamp)));
	}

	total_len += snprintf(buff + total_len, buffSize - total_len,
			"sorted storage servers:\n");
	ppServerEnd = g_sorted_storages + g_storage_count;
	for (ppServer=g_sorted_storages; ppServer<ppServerEnd; ppServer++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len,
			"\t%d. server: %s\n", 
			(int)(ppServer - g_sorted_storages) + 1, 
			(*ppServer)->server.ip_addr);
	}

	return total_len;
}

static int fdfs_dump_storage_stat(char *buff, const int buffSize)
{
	int total_len;
	char szLastHeartBeatTime[32];
	char szSrcUpdTime[32];
	char szSyncUpdTime[32];
	char szSyncedTimestamp[32];

	total_len = snprintf(buff, buffSize,
		"\ng_stat_change_count=%d\n"
		"g_sync_change_count=%d\n"
		"alloc_count=%d\n"
		"current_count=%d\n"
		"max_count=%d\n"
		"total_upload_count=%"PRId64"\n"
		"success_upload_count=%"PRId64"\n"
		"total_set_meta_count=%"PRId64"\n"
		"success_set_meta_count=%"PRId64"\n"
		"total_delete_count=%"PRId64"\n"
		"success_delete_count=%"PRId64"\n"
		"total_download_count=%"PRId64"\n"
		"success_download_count=%"PRId64"\n"
		"total_get_meta_count=%"PRId64"\n"
		"success_get_meta_count=%"PRId64"\n"
		"total_create_link_count=%"PRId64"\n"
		"success_create_link_count=%"PRId64"\n"
		"total_delete_link_count=%"PRId64"\n"
		"success_delete_link_count=%"PRId64"\n"
		"last_source_update=%s\n"
		"last_sync_update=%s\n"
		"last_synced_timestamp=%s\n"
		"last_heart_beat_time=%s\n",
		g_stat_change_count, g_sync_change_count,
        free_queue_alloc_connections(),
		g_storage_stat.connection.current_count,
		g_storage_stat.connection.max_count,
		g_storage_stat.total_upload_count,
		g_storage_stat.success_upload_count,
		g_storage_stat.total_set_meta_count,
		g_storage_stat.success_set_meta_count,
		g_storage_stat.total_delete_count,
		g_storage_stat.success_delete_count,
		g_storage_stat.total_download_count,
		g_storage_stat.success_download_count,
		g_storage_stat.total_get_meta_count,
		g_storage_stat.success_get_meta_count,
		g_storage_stat.total_create_link_count,
		g_storage_stat.success_create_link_count,
		g_storage_stat.total_delete_link_count,
		g_storage_stat.success_delete_link_count,
		formatDatetime(g_storage_stat.last_source_update, 
			"%Y-%m-%d %H:%M:%S", 
			szSrcUpdTime, sizeof(szSrcUpdTime)), 
		formatDatetime(g_storage_stat.last_sync_update,
			"%Y-%m-%d %H:%M:%S", 
			szSyncUpdTime, sizeof(szSyncUpdTime)), 
		formatDatetime(g_storage_stat.last_synced_timestamp, 
			"%Y-%m-%d %H:%M:%S", 
			szSyncedTimestamp, sizeof(szSyncedTimestamp)),
		formatDatetime(g_storage_stat.last_heart_beat_time,
			"%Y-%m-%d %H:%M:%S", 
			szLastHeartBeatTime, sizeof(szLastHeartBeatTime)));

	return total_len;
}

#define WRITE_TO_FILE(fd, buff, len) \
	if (write(fd, buff, len) != len) \
	{ \
		logError("file: "__FILE__", line: %d, " \
			"write to file %s fail, errno: %d, error info: %s", \
			__LINE__, filename, errno, STRERROR(errno)); \
		result = errno; \
		break; \
	}

int fdfs_dump_storage_global_vars_to_file(const char *filename)
{
	char buff[4 * 1024];
	char szCurrentTime[32];
	int len;
	int result;
	int fd;

	fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0)
	{
		logError("file: "__FILE__", line: %d, "
			"open file %s fail, errno: %d, error info: %s",
			__LINE__, filename, errno, STRERROR(errno));
		return errno;
	}

	do
	{
		result = 0;
		formatDatetime(g_current_time, "%Y-%m-%d %H:%M:%S", 
				szCurrentTime, sizeof(szCurrentTime));

		len = sprintf(buff, "\n====time: %s  DUMP START====\n", 
				szCurrentTime);
		WRITE_TO_FILE(fd, buff, len)

		len = fdfs_dump_global_vars(buff, sizeof(buff));
		WRITE_TO_FILE(fd, buff, len)

		len = fdfs_dump_tracker_servers(buff, sizeof(buff));
		WRITE_TO_FILE(fd, buff, len)

		len = fdfs_dump_storage_stat(buff, sizeof(buff));
		WRITE_TO_FILE(fd, buff, len)

		len = fdfs_dump_storage_servers(buff, sizeof(buff));
		WRITE_TO_FILE(fd, buff, len)

		len = sprintf(buff, "\n====time: %s  DUMP END====\n\n", 
				szCurrentTime);
		WRITE_TO_FILE(fd, buff, len)
	} while(0);

	close(fd);

	return result;
}

