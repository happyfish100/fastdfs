#!/bin/bash
#
# fastdfs storage
#
#
# @date 2020/04/26 18:10
# @author  ch
###########################

if [ ! -n "$TR_SERVER" ]; then
  echo "tr_server is null"
  exit 1
fi

if [ ! -n "$SERVER_PORT" ]; then
  echo "server_port is null"
  exit 1
fi

if [ ! -n "$GROUP_NAME" ]; then
  echo "group_name is null"
  echo "use default group_name"
  GROUP_NAME="group1"
fi

mkdir -p /home/dfs/logs
echo 'start storage ' > /home/dfs/logs/storaged.log

# 创建基本配置文件
mkdir -p ./conf

echo "# create storage config"
echo "# is this config file disabled" >> ./conf/storage.conf
echo "# false for enabled" >> ./conf/storage.conf
echo "# true for disabled" >> ./conf/storage.conf
echo "disabled=false" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# 该存储服务器所属的组的名称" >> ./conf/storage.conf
echo "#" >> ./conf/storage.conf
echo "# 评论或删除此项目以从跟踪服务器获取," >> ./conf/storage.conf
echo "# 在这种情况下，必须在tracker.conf中将use_storage_id设置为true。," >> ./conf/storage.conf
echo "# 和storage_ids.conf必须正确配置." >> ./conf/storage.conf
echo "group_name="$GROUP_NAME >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# bind an address of this host" >> ./conf/storage.conf
echo "# empty for bind all addresses of this host" >> ./conf/storage.conf
echo "bind_addr=" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# if bind an address of this host when connect to other servers" >> ./conf/storage.conf
echo "# (this storage server as a client)" >> ./conf/storage.conf
echo "# true for binding the address configed by above parameter: "bind_addr"" >> ./conf/storage.conf
echo "# false for binding any address of this host" >> ./conf/storage.conf
echo "client_bind=true" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# the storage server port" >> ./conf/storage.conf
echo "port="$SERVER_PORT >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# connect timeout in seconds" >> ./conf/storage.conf
echo "# default value is 30s" >> ./conf/storage.conf
echo "connect_timeout=10" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# network timeout in seconds" >> ./conf/storage.conf
echo "# default value is 30s" >> ./conf/storage.conf
echo "network_timeout=60" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# heart beat interval in seconds" >> ./conf/storage.conf
echo "heart_beat_interval=30" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# disk usage report interval in seconds" >> ./conf/storage.conf
echo "stat_report_interval=60" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# the base path to store data and log files" >> ./conf/storage.conf
echo "base_path=/home/dfs" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# max concurrent connections the server supported" >> ./conf/storage.conf
echo "# default value is 256" >> ./conf/storage.conf
echo "# more max_connections means more memory will be used" >> ./conf/storage.conf
echo "# you should set this parameter larger, eg. 10240" >> ./conf/storage.conf
echo "max_connections=1024" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# the buff size to recv / send data" >> ./conf/storage.conf
echo "# this parameter must more than 8KB" >> ./conf/storage.conf
echo "# default value is 64KB" >> ./conf/storage.conf
echo "# since V2.00" >> ./conf/storage.conf
echo "buff_size = 256KB" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# accept thread count" >> ./conf/storage.conf
echo "# default value is 1" >> ./conf/storage.conf
echo "# since V4.07" >> ./conf/storage.conf
echo "accept_threads=1" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# work thread count, should <= max_connections" >> ./conf/storage.conf
echo "# work thread deal network io" >> ./conf/storage.conf
echo "# default value is 4" >> ./conf/storage.conf
echo "# since V2.00" >> ./conf/storage.conf
echo "work_threads=4" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# if disk read / write separated" >> ./conf/storage.conf
echo "# false for mixed read and write" >> ./conf/storage.conf
echo "# true for separated read and write" >> ./conf/storage.conf
echo "# default value is true" >> ./conf/storage.conf
echo "# since V2.00" >> ./conf/storage.conf
echo "disk_rw_separated = true" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# disk reader thread count per store base path" >> ./conf/storage.conf
echo "# for mixed read / write, this parameter can be 0" >> ./conf/storage.conf
echo "# default value is 1" >> ./conf/storage.conf
echo "# since V2.00" >> ./conf/storage.conf
echo "disk_reader_threads = 1" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# disk writer thread count per store base path" >> ./conf/storage.conf
echo "# for mixed read / write, this parameter can be 0" >> ./conf/storage.conf
echo "# default value is 1" >> ./conf/storage.conf
echo "# since V2.00" >> ./conf/storage.conf
echo "disk_writer_threads = 1" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# when no entry to sync, try read binlog again after X milliseconds" >> ./conf/storage.conf
echo "# must > 0, default value is 200ms" >> ./conf/storage.conf
echo "sync_wait_msec=50" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# after sync a file, usleep milliseconds" >> ./conf/storage.conf
echo "# 0 for sync successively (never call usleep)" >> ./conf/storage.conf
echo "sync_interval=0" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# storage sync start time of a day, time format: Hour:Minute" >> ./conf/storage.conf
echo "# Hour from 0 to 23, Minute from 0 to 59" >> ./conf/storage.conf
echo "sync_start_time=00:00" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# storage sync end time of a day, time format: Hour:Minute" >> ./conf/storage.conf
echo "# Hour from 0 to 23, Minute from 0 to 59" >> ./conf/storage.conf
echo "sync_end_time=23:59" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# write to the mark file after sync N files" >> ./conf/storage.conf
echo "# default value is 500" >> ./conf/storage.conf
echo "write_mark_file_freq=500" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# path(disk or mount point) count, default value is 1" >> ./conf/storage.conf
echo "store_path_count=1" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# store_path#, based 0, if store_path0 not exists, it's value is base_path" >> ./conf/storage.conf
echo "# the paths must be exist" >> ./conf/storage.conf
echo "store_path0=/home/dfs" >> ./conf/storage.conf
echo "#store_path1=/home/dfs2" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# subdir_count  * subdir_count directories will be auto created under each" >> ./conf/storage.conf
echo "# store_path (disk), value can be 1 to 256, default value is 256" >> ./conf/storage.conf
echo "subdir_count_per_path=256" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "#standard log level as syslog, case insensitive, value list:" >> ./conf/storage.conf
echo "### emerg for emergency" >> ./conf/storage.conf
echo "### alert" >> ./conf/storage.conf
echo "### crit for critical" >> ./conf/storage.conf
echo "### error" >> ./conf/storage.conf
echo "### warn for warning" >> ./conf/storage.conf
echo "### notice" >> ./conf/storage.conf
echo "### info" >> ./conf/storage.conf
echo "### debug" >> ./conf/storage.conf
echo "log_level=info" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "#unix group name to run this program," >> ./conf/storage.conf
echo "#not set (empty) means run by the group of current user" >> ./conf/storage.conf
echo "run_by_group=" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "#unix username to run this program," >> ./conf/storage.conf
echo "#not set (empty) means run by current user" >> ./conf/storage.conf
echo "run_by_user=" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# allow_hosts can ocur more than once, host can be hostname or ip address," >> ./conf/storage.conf
echo "# '*' (only one asterisk) means match all ip addresses" >> ./conf/storage.conf
echo "# we can use CIDR ips like 192.168.5.64/26" >> ./conf/storage.conf
echo "# and also use range like these: 10.0.1.[0-254] and host[01-08,20-25].domain.com" >> ./conf/storage.conf
echo "# for example:" >> ./conf/storage.conf
echo "# allow_hosts=10.0.1.[1-15,20]" >> ./conf/storage.conf
echo "# allow_hosts=host[01-08,20-25].domain.com" >> ./conf/storage.conf
echo "# allow_hosts=192.168.5.64/26" >> ./conf/storage.conf
echo "allow_hosts=*" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# the mode of the files distributed to the data path" >> ./conf/storage.conf
echo "# 0: round robin(default)" >> ./conf/storage.conf
echo "# 1: random, distributted by hash code" >> ./conf/storage.conf
echo "file_distribute_path_mode=0" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# valid when file_distribute_to_path is set to 0 (round robin)," >> ./conf/storage.conf
echo "# when the written file count reaches this number, then rotate to next path" >> ./conf/storage.conf
echo "# default value is 100" >> ./conf/storage.conf
echo "file_distribute_rotate_count=100" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# call fsync to disk when write big file" >> ./conf/storage.conf
echo "# 0: never call fsync" >> ./conf/storage.conf
echo "# other: call fsync when written bytes >= this bytes" >> ./conf/storage.conf
echo "# default value is 0 (never call fsync)" >> ./conf/storage.conf
echo "fsync_after_written_bytes=0" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# sync log buff to disk every interval seconds" >> ./conf/storage.conf
echo "# must > 0, default value is 10 seconds" >> ./conf/storage.conf
echo "sync_log_buff_interval=10" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# sync binlog buff / cache to disk every interval seconds" >> ./conf/storage.conf
echo "# default value is 60 seconds" >> ./conf/storage.conf
echo "sync_binlog_buff_interval=10" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# sync storage stat info to disk every interval seconds" >> ./conf/storage.conf
echo "# default value is 300 seconds" >> ./conf/storage.conf
echo "sync_stat_file_interval=300" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# thread stack size, should >= 512KB" >> ./conf/storage.conf
echo "# default value is 512KB" >> ./conf/storage.conf
echo "thread_stack_size=512KB" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# the priority as a source server for uploading file." >> ./conf/storage.conf
echo "# the lower this value, the higher its uploading priority." >> ./conf/storage.conf
echo "# default value is 10" >> ./conf/storage.conf
echo "upload_priority=10" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# the NIC alias prefix, such as eth in Linux, you can see it by ifconfig -a" >> ./conf/storage.conf
echo "# multi aliases split by comma. empty value means auto set by OS type" >> ./conf/storage.conf
echo "# default values is empty" >> ./conf/storage.conf
echo "if_alias_prefix=" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# if check file duplicate, when set to true, use FastDHT to store file indexes" >> ./conf/storage.conf
echo "# 1 or yes: need check" >> ./conf/storage.conf
echo "# 0 or no: do not check" >> ./conf/storage.conf
echo "# default value is 0" >> ./conf/storage.conf
echo "check_file_duplicate=0" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# file signature method for check file duplicate" >> ./conf/storage.conf
echo "## hash: four 32 bits hash code" >> ./conf/storage.conf
echo "## md5: MD5 signature" >> ./conf/storage.conf
echo "# default value is hash" >> ./conf/storage.conf
echo "# since V4.01" >> ./conf/storage.conf
echo "file_signature_method=hash" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# namespace for storing file indexes (key-value pairs)" >> ./conf/storage.conf
echo "# this item must be set when check_file_duplicate is true / on" >> ./conf/storage.conf
echo "key_namespace=FastDFS" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# set keep_alive to 1 to enable persistent connection with FastDHT servers" >> ./conf/storage.conf
echo "# default value is 0 (short connection)" >> ./conf/storage.conf
echo "keep_alive=0" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# you can use #include filename (not include double quotes) directive to" >> ./conf/storage.conf
echo "# load FastDHT server list, when the filename is a relative path such as" >> ./conf/storage.conf
echo "# pure filename, the base path is the base path of current/this config file." >> ./conf/storage.conf
echo "# must set FastDHT server list when check_file_duplicate is true / on" >> ./conf/storage.conf
echo "# please see INSTALL of FastDHT for detail" >> ./conf/storage.conf
echo "##include /home/yuqing/fastdht/conf/fdht_servers.conf" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# if log to access log" >> ./conf/storage.conf
echo "# default value is false" >> ./conf/storage.conf
echo "# since V4.00" >> ./conf/storage.conf
echo "use_access_log = false" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# if rotate the access log every day" >> ./conf/storage.conf
echo "# default value is false" >> ./conf/storage.conf
echo "# since V4.00" >> ./conf/storage.conf
echo "rotate_access_log = false" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# rotate access log time base, time format: Hour:Minute" >> ./conf/storage.conf
echo "# Hour from 0 to 23, Minute from 0 to 59" >> ./conf/storage.conf
echo "# default value is 00:00" >> ./conf/storage.conf
echo "# since V4.00" >> ./conf/storage.conf
echo "access_log_rotate_time=00:00" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# if rotate the error log every day" >> ./conf/storage.conf
echo "# default value is false" >> ./conf/storage.conf
echo "# since V4.02" >> ./conf/storage.conf
echo "rotate_error_log = false" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# rotate error log time base, time format: Hour:Minute" >> ./conf/storage.conf
echo "# Hour from 0 to 23, Minute from 0 to 59" >> ./conf/storage.conf
echo "# default value is 00:00" >> ./conf/storage.conf
echo "# since V4.02" >> ./conf/storage.conf
echo "error_log_rotate_time=00:00" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# rotate access log when the log file exceeds this size" >> ./conf/storage.conf
echo "# 0 means never rotates log file by log file size" >> ./conf/storage.conf
echo "# default value is 0" >> ./conf/storage.conf
echo "# since V4.02" >> ./conf/storage.conf
echo "rotate_access_log_size = 0" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# rotate error log when the log file exceeds this size" >> ./conf/storage.conf
echo "# 0 means never rotates log file by log file size" >> ./conf/storage.conf
echo "# default value is 0" >> ./conf/storage.conf
echo "# since V4.02" >> ./conf/storage.conf
echo "rotate_error_log_size = 0" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# keep days of the log files" >> ./conf/storage.conf
echo "# 0 means do not delete old log files" >> ./conf/storage.conf
echo "# default value is 0" >> ./conf/storage.conf
echo "log_file_keep_days = 0" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# if skip the invalid record when sync file" >> ./conf/storage.conf
echo "# default value is false" >> ./conf/storage.conf
echo "# since V4.02" >> ./conf/storage.conf
echo "file_sync_skip_invalid_record=false" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# if use connection pool" >> ./conf/storage.conf
echo "# default value is false" >> ./conf/storage.conf
echo "# since V4.05" >> ./conf/storage.conf
echo "use_connection_pool = false" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# connections whose the idle time exceeds this time will be closed" >> ./conf/storage.conf
echo "# unit: second" >> ./conf/storage.conf
echo "# default value is 3600" >> ./conf/storage.conf
echo "# since V4.05" >> ./conf/storage.conf
echo "connection_pool_max_idle_time = 3600" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# use the ip address of this storage server if domain_name is empty," >> ./conf/storage.conf
echo "# else this domain name will ocur in the url redirected by the tracker server" >> ./conf/storage.conf
echo "http.domain_name=" >> ./conf/storage.conf
echo "" >> ./conf/storage.conf

echo "# the port of the web server on this storage server" >> ./conf/storage.conf
echo "http.server_port=8888" >> ./conf/storage.conf

# split
arr=(${TR_SERVER//,/ })

# tracker
echo "#tracker_server can ocur more than once, and tracker_server format is" >> ./conf/storage.conf
echo "#host:port, host can be hostname or ip address" >> ./conf/storage.conf

for var in ${arr[@]}
do
  echo "tracker_server="$var >> ./conf/storage.conf
done

# 移动配置文件
cat ./conf/storage.conf >  /etc/fdfs/storage.conf

# start
/etc/init.d/fdfs_storaged start

# cat log
tail -f /home/dfs/logs/storaged.log
