#!/bin/sh
#
# 用途：配置tracker \ storage的配置文件参数，liyanjing 2022.08.10
#


# 1. tracker 主要参数，生产环境中建议更改一下端口
tracker_port=22122
# 实现互备，两台tracker就够了
tracker_server="tracker_server = 172.16.100.90:$tracker_port\ntracker_server = 172.16.100.91:$tracker_port"

# 格式：<id>  <group_name>  <ip_or_hostname
storage_ids="
100001   group1  172.16.100.90
100002   group2  172.16.100.91
"

# 设置tracker访问IP限制，避免谁都能上传文件，默认是allow_hosts = *
allow_hosts="allow_hosts = 172.16.100.[85-91,83]\n"

# 2. local storage 主要参数，生产环境中建议更改一下端口
storage_group_name="group1"
storage_server_port=23000
store_path_count=1   #文件存储目录的个数，存储目录约定为/data/fastdfs/upload/path0~n

#==================以下是方法体================================
function tracker_confset() {

  sed -i "s|^port =.*$|port = $tracker_port|g" ./conf/tracker.conf
  # use storage ID instead of IP address
  sed -i "s|^use_storage_id =.*$|use_storage_id = true|g" ./conf/tracker.conf

cat > ./conf/storage_ids.conf << EOF
# <id>  <group_name>  <ip_or_hostname[:port]>
#
# id is a natural number (1, 2, 3 etc.),
# 6 bits of the id length is enough, such as 100001
#
# storage ip or hostname can be dual IPs seperated by comma,
# one is an inner (intranet) IP and another is an outer (extranet) IP,
# or two different types of inner (intranet) IPs
# for example: 192.168.2.100,122.244.141.46
# another eg.: 192.168.1.10,172.17.4.21
#
# the port is optional. if you run more than one storaged instances
# in a server, you must specified the port to distinguish different instances.

#100001   group1  192.168.0.196
#100002   group1  192.168.0.197
$storage_ids

EOF

  # 设置tracker访问IP限制，避免谁都能上传文件，默认是allow_hosts = *
  #sed -i '/^allow_hosts/{N;/^allow_hosts/s/.*/'"${allow_hosts}"'/}' ./conf/tracker.conf
}

function storage_confset() {

  #storage.conf 替换参数
  sed -i "s|^port =.*$|port = $storage_server_port|g" ./conf/storage.conf
  sed -i "s|^group_name =.*$|group_name = $storage_group_name|g" ./conf/storage.conf
  
  sed -i "s|^store_path_count =.*$|store_path_count = $store_path_count|g" ./conf/storage.conf
  arr_store_path="store_path0 = /data/fastdfs/upload/path0"
  for((i=1;i<$store_path_count;i++));
  do
       arr_store_path="$arr_store_path \nstore_path$i = /data/fastdfs/upload/path$i"
  done

  sed -i '/^store_path[1-9] =.*$/d' ./conf/storage.conf
  sed -i '/^#store_path[0-9] =.*$/d' ./conf/storage.conf
  sed -i "s|^store_path0 =.*$|$arr_store_path|g" ./conf/storage.conf
  
  sed -i "/^tracker_server =/{N;/^tracker_server =/s/.*/$tracker_server/}" ./conf/storage.conf

  #mod_fastdfs.conf 替换参数
  sed -i "/^tracker_server/{N;/^tracker_server/s/.*/$tracker_server/}" ./conf/mod_fastdfs.conf
  sed -i "s|^storage_server_port=.*$|storage_server_port=$storage_server_port|g" ./conf/mod_fastdfs.conf
  sed -i "s|^group_name=.*$|group_name=$storage_group_name|g" ./conf/mod_fastdfs.conf
  sed -i "s|^url_have_group_name =.*$|url_have_group_name = true|g" ./conf/mod_fastdfs.conf
  sed -i "s|^store_path_count=.*$|store_path_count=$store_path_count|g" ./conf/mod_fastdfs.conf
  sed -i '/^store_path[1-9].*/d' ./conf/mod_fastdfs.conf
  sed -i "s|^store_path0.*|$arr_store_path|g" ./conf/mod_fastdfs.conf
  sed -i "s|^use_storage_id =.*$|use_storage_id = true|g" ./conf/mod_fastdfs.conf

  #client.conf   当需要使用fastdfs自带的客户端时，更改此文件
  sed -i "/^tracker_server/{N;/^tracker_server/s/.*/$tracker_server/}" ./conf/client.conf
  sed -i "s|^use_storage_id =.*$|use_storage_id = true|g" ./conf/client.conf
 
}


mode_number=1
function chose_info_print() {
  echo -e "\033[32m 请先设置好本脚本的tracker \ storage 的参数变量，然后再选择：
  [1] 配置 tracker
  
  [2] 配置 storage\033[0m"
}

#执行
function run() {

  #1.屏幕打印出选择项
  chose_info_print 

  read -p "please input number 1 to 2: " mode_number
  if [[ ! $mode_number =~ [0-2]+ ]]; then
    echo -e "\033[31merror! the number you input isn't 1 to 2\n\033[0m"
    exit 1
  fi

  #2. 执行
  case ${mode_number} in
    1) 
     #echo "设置tracker"
     tracker_confset
      ;;
    2)
     #echo "设置storage"
     storage_confset
      ;;  
    *) 
      echo -e "\033[31merror! the number you input isn't 1 to 2\n\033[0m"
      ;;
  esac

  echo -e "\033[36m ${input_parameter} 配置文件设置完毕，建议人工复核一下\033[0m"

}

run


