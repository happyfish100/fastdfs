#!/bin/sh

# fastdfs 配置文件，我设置的存储路径，需要提前创建
FASTDFS_BASE_PATH=/data/fastdfs_data \
FASTDFS_STORE_PATH=/data/fastdfs/upload \

# 启用参数
# - tracker : 启动tracker_server 服务
# - storage : 启动storage 服务
start_parameter=$1

if [ ! -d "$FASTDFS_BASE_PATH" ];  then
	 mkdir -p ${FASTDFS_BASE_PATH};
fi	 

function start_tracker(){

  /usr/bin/fdfs_trackerd /etc/fdfs/tracker.conf
  tail -f /data/fastdfs_data/logs/trackerd.log
	 
}

function start_storage(){
  if [ ! -d "$FASTDFS_STORE_PATH" ]; then
	     mkdir -p ${FASTDFS_STORE_PATH}/{path0,path1,path2,path3};
  fi     
  /usr/bin/fdfs_storaged /etc/fdfs/storage.conf;
  sleep 5

  # nginx日志存储目录为/data/fastdfs_data/logs/，手动创建一下，防止storage启动慢，还没有来得及创建logs目录
  if [ ! -d "$FASTDFS_BASE_PATH/logs" ];  then
	 mkdir -p ${FASTDFS_BASE_PATH}/logs;
  fi
  
  /usr/local/nginx/sbin/nginx;
  tail -f /data/fastdfs_data/logs/storaged.log;
}

function run (){

  case ${start_parameter} in
    tracker)
     echo "启动tracker"
     start_tracker
    ;;
    storage)
       echo "启动storage"
       start_storage
    ;;
    *)
       echo "请指定要启动哪个服务，tracker还是storage（二选一），传参为tracker | storage"
  esac
}

run
