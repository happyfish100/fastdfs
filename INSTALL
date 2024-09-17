Copy right 2009 Happy Fish / YuQing

FastDFS may be copied only under the terms of the GNU General
Public License V3, which may be found in the FastDFS source kit.
Please visit the FastDFS Home Page for more detail.
Chinese language: http://www.fastken.com/

# step 1. download libfastcommon source codes and install it,
#   github address:  https://github.com/happyfish100/libfastcommon.git
#   gitee address:   https://gitee.com/fastdfs100/libfastcommon.git
# command lines as:

   git clone https://github.com/happyfish100/libfastcommon.git
   cd libfastcommon; git checkout V1.0.75
   ./make.sh clean && ./make.sh && ./make.sh install


# step 2. download libserverframe source codes and install it,
#   github address:  https://github.com/happyfish100/libserverframe.git
#   gitee address:   https://gitee.com/fastdfs100/libserverframe.git
# command lines as:

   git clone https://github.com/happyfish100/libserverframe.git
   cd libserverframe; git checkout V1.2.5
   ./make.sh clean && ./make.sh && ./make.sh install

# step 3. download fastdfs source codes and install it, 
#   github address:  https://github.com/happyfish100/fastdfs.git
#   gitee address:   https://gitee.com/fastdfs100/fastdfs.git
# command lines as:

   git clone https://github.com/happyfish100/fastdfs.git
   cd fastdfs; git checkout V6.12.2
   ./make.sh clean && ./make.sh && ./make.sh install


# step 4. setup the config files
#   the setup script does NOT overwrite existing config files,
#   please feel free to execute this script (take easy :)

./setup.sh /etc/fdfs


# step 5. edit or modify the config files of tracker, storage and client
such as:
 vi /etc/fdfs/tracker.conf
 vi /etc/fdfs/storage.conf
 vi /etc/fdfs/client.conf

 and so on ...


# step 6. run the server programs
# start the tracker server:
/usr/bin/fdfs_trackerd /etc/fdfs/tracker.conf restart

# start the storage server:
/usr/bin/fdfs_storaged /etc/fdfs/storage.conf restart

# (optional) in Linux, you can start fdfs_trackerd and fdfs_storaged as a service:
/sbin/service fdfs_trackerd restart
/sbin/service fdfs_storaged restart


# step 7. (optional) run monitor program
# such as:
/usr/bin/fdfs_monitor /etc/fdfs/client.conf


# step 8. (optional) run the test program
# such as:
/usr/bin/fdfs_test <client_conf_filename> <operation>
/usr/bin/fdfs_test1 <client_conf_filename> <operation>

# for example, upload a file for test:
/usr/bin/fdfs_test /etc/fdfs/client.conf upload /usr/include/stdlib.h


tracker server config file sample please see conf/tracker.conf

storage server config file sample please see conf/storage.conf

client config file sample please see conf/client.conf

Item detail
1. server common items
---------------------------------------------------
|  item name            |  type  | default | Must |
---------------------------------------------------
| base_path             | string |         |  Y   |
---------------------------------------------------
| disabled              | boolean| false   |  N   |
---------------------------------------------------
| bind_addr             | string |         |  N   |
---------------------------------------------------
| network_timeout       | int    | 30(s)   |  N   |
---------------------------------------------------
| max_connections       | int    | 256     |  N   |
---------------------------------------------------
| log_level             | string | info    |  N   |
---------------------------------------------------
| run_by_group          | string |         |  N   |
---------------------------------------------------
| run_by_user           | string |         |  N   |
---------------------------------------------------
| allow_hosts           | string |   *     |  N   |
---------------------------------------------------
| sync_log_buff_interval| int    |  10(s)  |  N   |
---------------------------------------------------
| thread_stack_size     | string |  1M     |  N   |
---------------------------------------------------
memo:
   * base_path is the base path of sub dirs: 
     data and logs. base_path must exist and it's sub dirs will 
     be automatically created if not exist.
       $base_path/data: store data files
       $base_path/logs: store log files
   * log_level is the standard log level as syslog, case insensitive
     # emerg: for emergency
     # alert
     # crit: for critical
     # error
     # warn: for warning
     # notice
     # info
     # debug
   * allow_hosts can ocur more than once, host can be hostname or ip address,
     "*" means match all ip addresses, can use range like this: 10.0.1.[1-15,20]
      or host[01-08,20-25].domain.com, for example:
        allow_hosts=10.0.1.[1-15,20]
        allow_hosts=host[01-08,20-25].domain.com

2. tracker server items
---------------------------------------------------
|  item name            |  type  | default | Must |
---------------------------------------------------
| port                  | int    | 22000   |  N   |
---------------------------------------------------
| store_lookup          | int    |  0      |  N   |
---------------------------------------------------
| store_group           | string |         |  N   |
---------------------------------------------------
| store_server          | int    |  0      |  N   |
---------------------------------------------------
| store_path            | int    |  0      |  N   |
---------------------------------------------------
| download_server       | int    |  0      |  N   |
---------------------------------------------------
| reserved_storage_space| string |  1GB    |  N   |
---------------------------------------------------

memo: 
  * the value of store_lookup is:
    0: round robin (default)
    1: specify group
    2: load balance (supported since V1.1)
  * store_group is the name of group to store files.
    when store_lookup set to 1(specify group), 
    store_group must be set to a specified group name.
  * reserved_storage_space is the reserved storage space for system 
    or other applications. if the free(available) space of any stoarge
    server in a group <= reserved_storage_space, no file can be uploaded
    to this group (since V1.1)
    bytes unit can be one of follows:
      # G or g for gigabyte(GB)
      # M or m for megabyte(MB)
      # K or k for kilobyte(KB)
      # no unit for byte(B)

3. storage server items
-------------------------------------------------
|  item name          |  type  | default | Must |
-------------------------------------------------
| group_name          | string |         |  Y   |
-------------------------------------------------
| tracker_server      | string |         |  Y   |
-------------------------------------------------
| port                | int    | 23000   |  N   |
-------------------------------------------------
| heart_beat_interval | int    |  30(s)  |  N   |
-------------------------------------------------
| stat_report_interval| int    | 300(s)  |  N   |
-------------------------------------------------
| sync_wait_msec      | int    | 100(ms) |  N   |
-------------------------------------------------
| sync_interval       | int    |   0(ms) |  N   |
-------------------------------------------------
| sync_start_time     | string |  00:00  |  N   |
-------------------------------------------------
| sync_end_time       | string |  23:59  |  N   |
-------------------------------------------------
| store_path_count    | int    |   1     |  N   |
-------------------------------------------------
| store_path0         | string |base_path|  N   |
-------------------------------------------------
| store_path#         | string |         |  N   |
-------------------------------------------------
|subdir_count_per_path| int    |   256   |  N   |
-------------------------------------------------
|check_file_duplicate | boolean|    0    |  N   |
-------------------------------------------------
| key_namespace       | string |         |  N   |
-------------------------------------------------
| keep_alive          | boolean|    0    |  N   |
-------------------------------------------------
| sync_binlog_buff_interval| int |   60s |  N   |
-------------------------------------------------

memo:
  * tracker_server can ocur more than once, and tracker_server format is
    "host:port", host can be hostname or ip address.
  * store_path#, # for digital, based 0
  * check_file_duplicate: when set to true, must work with FastDHT server, 
    more detail please see INSTALL of FastDHT. FastDHT download page: 
    http://code.google.com/p/fastdht/downloads/list
  * key_namespace: FastDHT key namespace, can't be empty when 
    check_file_duplicate is true. the key namespace should short as possible
 
