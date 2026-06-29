
# 推荐的FastDFS部署方案
* 使用V4引入的storage server ID特性
* 如果以小文件为主（比如文件平均大小小于200K）且单个存储目录的文件数总量将超过一千万，推荐启用合并存储特性
* 不要做RAID，直接挂载单盘，每个硬盘一个mount point作为FastDFS的一个store path
* 文件上传和删除等操作：使用FastDFS client API，官方提供了C、PHP extension和Java的client API
* 文件下载采用HTTP方式：使用nginx扩展模块fastdfs-nginx-module

## 使用storage server ID特性
* 配置文件：tracker.conf
* use_storage_id：是否启用storage server ID特性，默认值为false，需设置为true
* id_type_in_filename：storage server生成的文件ID中包含的是server ID还是IP地址，使用默认配置的id即可
* 说明：使用storage server ID可以避免服务器IP变更带来的问题，可以在任意时间点开启本特性，请放心启动
* 特别提示：如果需要在一台服务器上部署同组的多个storage实例，必须启用storage server ID特性并且把id_type_in_filename配置为id

## 使用小文件合并存储特性
* 配置文件：tracker.conf
* use_trunk_file：是否启用合并存储特性，默认值为false，需设置为true
* trunk_file_size：trunk文件大小，默认值为64MB，通常采用默认值即可
* 友情提示：可以在任意时间点开启小文件合并存储特性，一旦开启合并存储后请不要关闭此特性（即回退为不启用），否则在删除旧文件时会有问题

## 最大并发连接数设置
* 配置文件：tracker.conf 和 storage.conf
* 参数名：max_connections
* 缺省值：256
* 说明： FastDFS网络通信buffer数采用内存池，该内存池使用增量(按需)分配方式，强烈建议配置得大一些，线上环境至少配置10240，避免tracker或storage服务的并发连接数超过max_connections
* 特别提示：
```
  用fdfs_monitor和fdfs_tracker_stat看到连接统计中的三个指标含义：
    alloc_count 内存池预分配的buffer数
    current_count 当前连接数（正在使用buffer数）
    max_count 曾经达到过的最大连接数，max_count小于等于alloc_count，二者数值相差不会超过一次预分配的buffer数（比如64或256）

    fdfs_tracker_stat输出片段示例： connections {alloc: 256, current: 2, max: 23}
```

## 网络线程数设置
* 配置文件：tracker.conf 和 storage.conf
* 参数名：work_threads
* 缺省值：4
* 说明：为了避免CPU上下文切换的开销，以及不必要的资源消耗，不建议将本参数设置得过大，通常采用默认值4即可，不建议超过16

## storage磁盘读写线程设置
* 配置文件：storage.conf
* disk_rw_separated：磁盘读写是否分离，默认值为true
* disk_reader_threads：单个磁盘读线程数
* disk_writer_threads：单个磁盘写线程数
* 如果磁盘读写混合，单个磁盘读写线程数为读线程数和写线程数之和
* 对于单盘挂载方式，对于SATA等普通硬盘，磁盘读写线程分别设置为1即可；对于SAS或者SSD可以适当加大读写线程数
* 如果磁盘做了RAID，或者使用NFS等网络存储方式，需要酌情加大读写线程数，这样才能最大程度地发挥磁盘性能

## storage文件同步延迟相关设置
* 配置文件：storage.conf
* sync_binlog_buff_interval：将binlog buffer写入磁盘的时间间隔（单位为秒），取值大于0，缺省值为60，建议配置为1
* sync_wait_msec：如果没有需要同步的文件，对binlog进行轮询的时间间隔，取值大于0，缺省值为100ms
* sync_interval：同步完一个文件后，休眠的毫秒数，缺省值为0
* sync_min_threads：缺省值为1，通常采用这个默认值即可
* sync_max_threads：缺省值为auto表示为store_path_count的两倍
* 为了缩短文件同步时间，可以将前面3个参数适当调小；适当加大sync_max_threads来增加同步文件的线程数

## 存储目录数设置
* 配置文件：storage.conf
* 参数名：subdir_count_per_path
* 缺省值：256
* 说明：FastDFS采用二级目录的做法，目录会在FastDFS初始化时自动创建。存储海量小文件，打开了合并存储的情况下，建议将本参数适当改小，比如设置为32，此时存放文件的目录数为 32 * 32 = 1024。假如trunk文件大小采用缺省值64MB，磁盘空间为2TB，那么每个目录下存放的trunk文件数均值为：2TB / (1024 * 64MB) = 32个
* 友情提示：本参数的最大值为256，一旦设置后可以增大，但不能调小

