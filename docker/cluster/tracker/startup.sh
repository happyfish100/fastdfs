#!/bin/bash

if [ ! -n "$SERVER_PORT" ]; then
  echo "server_port is null"
  exit 1
fi

if [ ! -n "$LOOKUP" ]; then
  echo "lookup is null"
  echo "use default lookup"
  LOOKUP=0
fi

mkdir -p ./conf

echo "create tracker config " > ./conf/tracker.conf

echo "# is this config file disabled" >> ./conf/tracker.conf
echo "# false for enabled" >> ./conf/tracker.conf
echo "# true for disabled" >> ./conf/tracker.conf
echo "disabled=false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# bind an address of this host" >> ./conf/tracker.conf
echo "# empty for bind all addresses of this host" >> ./conf/tracker.conf
echo "bind_addr=" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the tracker server port" >> ./conf/tracker.conf
echo "port="$SERVER_PORT >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# connect timeout in seconds" >> ./conf/tracker.conf
echo "# default value is 30s" >> ./conf/tracker.conf
echo "connect_timeout=10" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# network timeout in seconds" >> ./conf/tracker.conf
echo "# default value is 30s" >> ./conf/tracker.conf
echo "network_timeout=60" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the base path to store data and log files" >> ./conf/tracker.conf
echo "base_path=/home/dfs" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# max concurrent connections this server supported" >> ./conf/tracker.conf
echo "# you should set this parameter larger, eg. 102400" >> ./conf/tracker.conf
echo "max_connections=1024" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# accept thread count" >> ./conf/tracker.conf
echo "# default value is 1" >> ./conf/tracker.conf
echo "# since V4.07" >> ./conf/tracker.conf
echo "accept_threads=1" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# work thread count, should <= max_connections" >> ./conf/tracker.conf
echo "# default value is 4" >> ./conf/tracker.conf
echo "# since V2.00" >> ./conf/tracker.conf
echo "work_threads=4" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# min buff size" >> ./conf/tracker.conf
echo "# default value 8KB" >> ./conf/tracker.conf
echo "min_buff_size = 8KB" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# max buff size" >> ./conf/tracker.conf
echo "# default value 128KB" >> ./conf/tracker.conf
echo "max_buff_size = 128KB" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the method of selecting group to upload files" >> ./conf/tracker.conf
echo "# 0: round robin" >> ./conf/tracker.conf
echo "# 1: specify group" >> ./conf/tracker.conf
echo "# 2: load balance, select the max free space group to upload file" >> ./conf/tracker.conf
echo "store_lookup="$LOOKUP >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# which group to upload file" >> ./conf/tracker.conf
echo "# when store_lookup set to 1, must set store_group to the group name" >> ./conf/tracker.conf
echo "store_group=group2" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# which storage server to upload file" >> ./conf/tracker.conf
echo "# 0: round robin (default)" >> ./conf/tracker.conf
echo "# 1: the first server order by ip address" >> ./conf/tracker.conf
echo "# 2: the first server order by priority (the minimal)" >> ./conf/tracker.conf
echo "# Note: if use_trunk_file set to true, must set store_server to 1 or 2" >> ./conf/tracker.conf
echo "store_server=0" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# which path(means disk or mount point) of the storage server to upload file" >> ./conf/tracker.conf
echo "# 0: round robin" >> ./conf/tracker.conf
echo "# 2: load balance, select the max free space path to upload file" >> ./conf/tracker.conf
echo "store_path=2" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# which storage server to download file" >> ./conf/tracker.conf
echo "# 0: round robin (default)" >> ./conf/tracker.conf
echo "# 1: the source storage server which the current file uploaded to" >> ./conf/tracker.conf
echo "download_server=0" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# reserved storage space for system or other applications." >> ./conf/tracker.conf
echo "# if the free(available) space of any stoarge server in" >> ./conf/tracker.conf
echo "# a group <= reserved_storage_space," >> ./conf/tracker.conf
echo "# no file can be uploaded to this group." >> ./conf/tracker.conf
echo "# bytes unit can be one of follows:" >> ./conf/tracker.conf
echo "### G or g for gigabyte(GB)" >> ./conf/tracker.conf
echo "### M or m for megabyte(MB)" >> ./conf/tracker.conf
echo "### K or k for kilobyte(KB)" >> ./conf/tracker.conf
echo "### no unit for byte(B)" >> ./conf/tracker.conf
echo "### XX.XX% as ratio such as reserved_storage_space = 10%" >> ./conf/tracker.conf
echo "reserved_storage_space = 1%" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "#standard log level as syslog, case insensitive, value list:" >> ./conf/tracker.conf
echo "### emerg for emergency" >> ./conf/tracker.conf
echo "### alert" >> ./conf/tracker.conf
echo "### crit for critical" >> ./conf/tracker.conf
echo "### error" >> ./conf/tracker.conf
echo "### warn for warning" >> ./conf/tracker.conf
echo "### notice" >> ./conf/tracker.conf
echo "### info" >> ./conf/tracker.conf
echo "### debug" >> ./conf/tracker.conf
echo "log_level=info" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "#unix group name to run this program," >> ./conf/tracker.conf
echo "#not set (empty) means run by the group of current user" >> ./conf/tracker.conf
echo "run_by_group=" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "#unix username to run this program," >> ./conf/tracker.conf
echo "#not set (empty) means run by current user" >> ./conf/tracker.conf
echo "run_by_user=" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# allow_hosts can ocur more than once, host can be hostname or ip address," >> ./conf/tracker.conf
echo "# "*" (only one asterisk) means match all ip addresses" >> ./conf/tracker.conf
echo "# we can use CIDR ips like 192.168.5.64/26" >> ./conf/tracker.conf
echo "# and also use range like these: 10.0.1.[0-254] and host[01-08,20-25].domain.com" >> ./conf/tracker.conf
echo "# for example:" >> ./conf/tracker.conf
echo "# allow_hosts=10.0.1.[1-15,20]" >> ./conf/tracker.conf
echo "# allow_hosts=host[01-08,20-25].domain.com" >> ./conf/tracker.conf
echo "# allow_hosts=192.168.5.64/26" >> ./conf/tracker.conf
echo "allow_hosts=*" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# sync log buff to disk every interval seconds" >> ./conf/tracker.conf
echo "# default value is 10 seconds" >> ./conf/tracker.conf
echo "sync_log_buff_interval = 10" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# check storage server alive interval seconds" >> ./conf/tracker.conf
echo "check_active_interval = 120" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# thread stack size, should >= 64KB" >> ./conf/tracker.conf
echo "# default value is 64KB" >> ./conf/tracker.conf
echo "thread_stack_size = 64KB" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# auto adjust when the ip address of the storage server changed" >> ./conf/tracker.conf
echo "# default value is true" >> ./conf/tracker.conf
echo "storage_ip_changed_auto_adjust = true" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# storage sync file max delay seconds" >> ./conf/tracker.conf
echo "# default value is 86400 seconds (one day)" >> ./conf/tracker.conf
echo "# since V2.00" >> ./conf/tracker.conf
echo "storage_sync_file_max_delay = 86400" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the max time of storage sync a file" >> ./conf/tracker.conf
echo "# default value is 300 seconds" >> ./conf/tracker.conf
echo "# since V2.00" >> ./conf/tracker.conf
echo "storage_sync_file_max_time = 300" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# if use a trunk file to store several small files" >> ./conf/tracker.conf
echo "# default value is false" >> ./conf/tracker.conf
echo "# since V3.00" >> ./conf/tracker.conf
echo "use_trunk_file = false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the min slot size, should <= 4KB" >> ./conf/tracker.conf
echo "# default value is 256 bytes" >> ./conf/tracker.conf
echo "# since V3.00" >> ./conf/tracker.conf
echo "slot_min_size = 256" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the max slot size, should > slot_min_size" >> ./conf/tracker.conf
echo "# store the upload file to trunk file when it's size <=  this value" >> ./conf/tracker.conf
echo "# default value is 16MB" >> ./conf/tracker.conf
echo "# since V3.00" >> ./conf/tracker.conf
echo "slot_max_size = 16MB" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the trunk file size, should >= 4MB" >> ./conf/tracker.conf
echo "# default value is 64MB" >> ./conf/tracker.conf
echo "# since V3.00" >> ./conf/tracker.conf
echo "trunk_file_size = 64MB" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# if create trunk file advancely" >> ./conf/tracker.conf
echo "# default value is false" >> ./conf/tracker.conf
echo "# since V3.06" >> ./conf/tracker.conf
echo "trunk_create_file_advance = false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the time base to create trunk file" >> ./conf/tracker.conf
echo "# the time format: HH:MM" >> ./conf/tracker.conf
echo "# default value is 02:00" >> ./conf/tracker.conf
echo "# since V3.06" >> ./conf/tracker.conf
echo "trunk_create_file_time_base = 02:00" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the interval of create trunk file, unit: second" >> ./conf/tracker.conf
echo "# default value is 38400 (one day)" >> ./conf/tracker.conf
echo "# since V3.06" >> ./conf/tracker.conf
echo "trunk_create_file_interval = 86400" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the threshold to create trunk file" >> ./conf/tracker.conf
echo "# when the free trunk file size less than the threshold, will create" >> ./conf/tracker.conf
echo "# the trunk files" >> ./conf/tracker.conf
echo "# default value is 0" >> ./conf/tracker.conf
echo "# since V3.06" >> ./conf/tracker.conf
echo "trunk_create_file_space_threshold = 20G" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# if check trunk space occupying when loading trunk free spaces" >> ./conf/tracker.conf
echo "# the occupied spaces will be ignored" >> ./conf/tracker.conf
echo "# default value is false" >> ./conf/tracker.conf
echo "# since V3.09" >> ./conf/tracker.conf
echo "# NOTICE: set this parameter to true will slow the loading of trunk spaces" >> ./conf/tracker.conf
echo "# when startup. you should set this parameter to true when neccessary." >> ./conf/tracker.conf
echo "trunk_init_check_occupying = false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# if ignore storage_trunk.dat, reload from trunk binlog" >> ./conf/tracker.conf
echo "# default value is false" >> ./conf/tracker.conf
echo "# since V3.10" >> ./conf/tracker.conf
echo "# set to true once for version upgrade when your version less than V3.10" >> ./conf/tracker.conf
echo "trunk_init_reload_from_binlog = false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# the min interval for compressing the trunk binlog file" >> ./conf/tracker.conf
echo "# unit: second" >> ./conf/tracker.conf
echo "# default value is 0, 0 means never compress" >> ./conf/tracker.conf
echo "# FastDFS compress the trunk binlog when trunk init and trunk destroy" >> ./conf/tracker.conf
echo "# recommand to set this parameter to 86400 (one day)" >> ./conf/tracker.conf
echo "# since V5.01" >> ./conf/tracker.conf
echo "trunk_compress_binlog_min_interval = 0" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# if use storage ID instead of IP address" >> ./conf/tracker.conf
echo "# default value is false" >> ./conf/tracker.conf
echo "# since V4.00" >> ./conf/tracker.conf
echo "use_storage_id = false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# specify storage ids filename, can use relative or absolute path" >> ./conf/tracker.conf
echo "# since V4.00" >> ./conf/tracker.conf
echo "storage_ids_filename = storage_ids.conf" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# id type of the storage server in the filename, values are:" >> ./conf/tracker.conf
echo "## ip: the ip address of the storage server" >> ./conf/tracker.conf
echo "## id: the server id of the storage server" >> ./conf/tracker.conf
echo "# this paramter is valid only when use_storage_id set to true" >> ./conf/tracker.conf
echo "# default value is ip" >> ./conf/tracker.conf
echo "# since V4.03" >> ./conf/tracker.conf
echo "id_type_in_filename = ip" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# if store slave file use symbol link" >> ./conf/tracker.conf
echo "# default value is false" >> ./conf/tracker.conf
echo "# since V4.01" >> ./conf/tracker.conf
echo "store_slave_file_use_link = false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# if rotate the error log every day" >> ./conf/tracker.conf
echo "# default value is false" >> ./conf/tracker.conf
echo "# since V4.02" >> ./conf/tracker.conf
echo "rotate_error_log = false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# rotate error log time base, time format: Hour:Minute" >> ./conf/tracker.conf
echo "# Hour from 0 to 23, Minute from 0 to 59" >> ./conf/tracker.conf
echo "# default value is 00:00" >> ./conf/tracker.conf
echo "# since V4.02" >> ./conf/tracker.conf
echo "error_log_rotate_time=00:00" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# rotate error log when the log file exceeds this size" >> ./conf/tracker.conf
echo "# 0 means never rotates log file by log file size" >> ./conf/tracker.conf
echo "# default value is 0" >> ./conf/tracker.conf
echo "# since V4.02" >> ./conf/tracker.conf
echo "rotate_error_log_size = 0" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# keep days of the log files" >> ./conf/tracker.conf
echo "# 0 means do not delete old log files" >> ./conf/tracker.conf
echo "# default value is 0" >> ./conf/tracker.conf
echo "log_file_keep_days = 0" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# if use connection pool" >> ./conf/tracker.conf
echo "# default value is false" >> ./conf/tracker.conf
echo "# since V4.05" >> ./conf/tracker.conf
echo "use_connection_pool = false" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# connections whose the idle time exceeds this time will be closed" >> ./conf/tracker.conf
echo "# unit: second" >> ./conf/tracker.conf
echo "# default value is 3600" >> ./conf/tracker.conf
echo "# since V4.05" >> ./conf/tracker.conf
echo "connection_pool_max_idle_time = 3600" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# HTTP port on this tracker server" >> ./conf/tracker.conf
echo "http.server_port=8080" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# check storage HTTP server alive interval seconds" >> ./conf/tracker.conf
echo "# <= 0 for never check" >> ./conf/tracker.conf
echo "# default value is 30" >> ./conf/tracker.conf
echo "http.check_alive_interval=30" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# check storage HTTP server alive type, values are:" >> ./conf/tracker.conf
echo "#   tcp : connect to the storge server with HTTP port only," >> ./conf/tracker.conf
echo "#        do not request and get response" >> ./conf/tracker.conf
echo "#   http: storage check alive url must return http status 200" >> ./conf/tracker.conf
echo "# default value is tcp" >> ./conf/tracker.conf
echo "http.check_alive_type=tcp" >> ./conf/tracker.conf
echo "" >> ./conf/tracker.conf

echo "# check storage HTTP server alive uri/url" >> ./conf/tracker.conf
echo "# NOTE: storage embed HTTP server support uri: /status.html" >> ./conf/tracker.conf
echo "http.check_alive_uri=/status.html" >> ./conf/tracker.conf

# 移动 配置文件
cat ./conf/tracker.conf > /etc/fdfs/tracker.conf

mkdir -p /home/dfs/logs
echo 'start tracker ' > /home/dfs/logs/trackerd.log

# start
/etc/init.d/fdfs_trackerd start

# cat log
tail -f  /home/dfs/logs/trackerd.log