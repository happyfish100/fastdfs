How to
=======================

文件及目录结构
-----------------------

FastDFS服务器端运行时目录结构如下：

::

  ${base_path}
    |__data：存放数据文件
    |__logs：存放日志文件

其中，${base_path}由配置文件中的参数“base_path”设定。

一、tracker server
^^^^^^^^^^^^^^^^^^^^^^^

tracker server目录及文件结构：

::

  ${base_path}
    |__data
    |     |__storage_groups.dat：存储分组信息
    |     |__storage_servers.dat：存储服务器列表
    |__logs
          |__trackerd.log：tracker server日志文件

数据文件storage_groups.dat和storage_servers.dat中的记录之间以换行符（\n）分隔，字段之间以西文逗号（,）分隔。
storage_groups.dat中的字段依次为：

1. group_name：组名
2. storage_port：storage server端口号

storage_servers.dat中记录storage server相关信息，字段依次为：

1. group_name：所属组名
2. ip_addr：ip地址
3. status：状态
4. sync_src_ip_addr：向该storage server同步已有数据文件的源服务器
5. sync_until_timestamp：同步已有数据文件的截至时间（UNIX时间戳）
6. stat.total_upload_count：上传文件次数
7. stat.success_upload_count：成功上传文件次数
8. stat.total_set_meta_count：更改meta data次数
9. stat.success_set_meta_count：成功更改meta data次数
10. stat.total_delete_count：删除文件次数
11. stat.success_delete_count：成功删除文件次数
12. stat.total_download_count：下载文件次数
13. stat.success_download_count：成功下载文件次数
14. stat.total_get_meta_count：获取meta data次数
15. stat.success_get_meta_count：成功获取meta data次数
16. stat.last_source_update：最近一次源头更新时间（更新操作来自客户端）
17. stat.last_sync_update：最近一次同步更新时间（更新操作来自其他storage server的同步）

二、storage server
^^^^^^^^^^^^^^^^^^^^^^^

storage server目录及文件结构：

::

  ${base_path}
    |__data
    |     |__.data_init_flag：当前storage server初始化信息
    |     |__storage_stat.dat：当前storage server统计信息
    |     |__sync：存放数据同步相关文件
    |     |     |__binlog.index：当前的binlog（更新操作日志）文件索引号
    |     |     |__binlog.###：存放更新操作记录（日志）
    |     |     |__${ip_addr}_${port}.mark：存放向目标服务器同步的完成情况
    |     |
    |     |__一级目录：256个存放数据文件的目录，目录名为十六进制字符，如：00, 1F
    |           |__二级目录：256个存放数据文件的目录，目录名为十六进制字符，如：0A, CF
    |__logs
          |__storaged.log：storage server日志文件

.data_init_flag文件格式为ini配置文件方式，各个参数如下：

::

  # storage_join_time：本storage server创建时间
  # sync_old_done：本storage server是否已完成同步的标志（源服务器向本服务器同步已有数据）
  # sync_src_server：向本服务器同步已有数据的源服务器IP地址，没有则为空
  # sync_until_timestamp：同步已有数据文件截至时间（UNIX时间戳）

storage_stat.dat文件格式为ini配置文件方式，各个参数如下：

::

  # total_upload_count：上传文件次数
  # success_upload_count：成功上传文件次数
  # total_set_meta_count：更改meta data次数
  # success_set_meta_count：成功更改meta data次数
  # total_delete_count：删除文件次数
  # success_delete_count：成功删除文件次数
  # total_download_count：下载文件次数
  # success_download_count：成功下载文件次数
  # total_get_meta_count：获取meta data次数
  # success_get_meta_count：成功获取meta data次数
  # last_source_update：最近一次源头更新时间（更新操作来自客户端）
  # last_sync_update：最近一次同步更新时间（更新操作来自其他storage server）

binlog.index中只有一个数据项：当前binlog的文件索引号

binlog.###，###为索引号对应的3位十进制字符，不足三位，前面补0。索引号基于0，最大为999。一个binlog文件最大为1GB。记录之间以换行符（\n）分隔，字段之间以西文空格分隔。字段依次为：

1. timestamp：更新发生时间（Unix时间戳）
2. op_type：操作类型，一个字符
3. filename：操作（更新）的文件名，包括相对路径，如：5A/3D/VKQ-CkpWmo0AAAAAAKqTJj0eiic6891.a

${ip_addr}_${port}.mark：ip_addr为同步的目标服务器IP地址，port为本组storage server端口。例如：10.0.0.1_23000.mark。文件格式为ini配置文件方式，各个参数如下：

::

  # binlog_index：已处理（同步）到的binlog索引号
  # binlog_offset：已处理（同步）到的binlog文件偏移量（字节数）
  # need_sync_old：同步已有数据文件标记，0表示没有数据文件需要同步
  # sync_old_done：同步已有数据文件是否完成标记，0表示未完成，1表示已完成
  # until_timestamp：同步已有数据截至时间点（UNIX时间戳）
  # scan_row_count：已扫描的binlog记录数
  # sync_row_count：已同步的binlog记录数

数据文件名由系统自动生成，包括5部分：存储服务器IP地址、当前时间（Unix时间戳）、文件大小（字节数）、随机数和文件后缀。文件名长度为33字节。文件可以按目录顺序存放，也可以按照PJW Hash算法hash到65536（256*256）个目录中分散存储，通过配置文件控制。

同步机制
-----------------------

在FastDFS的服务器端配置文件中，bind_addr这个参数用于需要绑定本机IP地址的场合。只有这个参数和主机特征相关，其余参数都是可以统一配置的。在不需要绑定本机的情况下，为了便于管理和维护，建议所有tracker server的配置文件相同，同组内的所有storage server的配置文件相同。

tracker server的配置文件中没有出现storage server，而storage server的配置文件中会列举出所有的tracker server。这就决定了storage server和tracker server之间的连接由storage server主动发起，storage server为每个tracker server启动一个线程进行连接和通讯，这部分的通信协议请参阅
《FastDFS HOWTO -- Protocol》
中的“2. storage server to tracker server command”部分。
tracker server会在内存中保存storage分组及各个组下的storage server，并将连接过自己的storage server及其分组保存到文件中，以便下次重启服务时能直接从本地磁盘中获得storage相关信息。storage server会在内存中记录本组的所有服务器，并将服务器信息记录到文件中。tracker server和storage server之间相互同步storage server列表：

1. 如果一个组内增加了新的storage server或者storage server的状态发生了改变，tracker server都会将storage server列表同步给该组内的所有storage server。以新增storage server为例，因为新加入的storage server主动连接tracker server，tracker server发现有新的storage server加入，就会将该组内所有的storage server返回给新加入的storage server，并重新将该组的storage server列表返回给该组内的其他storage server；
2. 如果新增加一台tracker server，storage server连接该tracker server，发现该tracker server返回的本组storage server列表比本机记录的要少，就会将该tracker server上没有的storage server同步给该tracker server。

同一组内的storage server之间是对等的，文件上传、删除等操作可以在任意一台storage server上进行。文件同步只在同组内的storage server之间进行，采用push方式，即源服务器同步给目标服务器。以文件上传为例，假设一个组内有3台storage server A、B和C，文件F上传到服务器B，由B将文件F同步到其余的两台服务器A和C。我们不妨把文件F上传到服务器B的操作为源头操作，在服务器B上的F文件为源头数据；文件F被同步到服务器A和C的操作为备份操作，在A和C上的F文件为备份数据。同步规则总结如下：

1. 只在本组内的storage server之间进行同步；
2. 源头数据才需要同步，备份数据不需要再次同步，否则就构成环路了；
3. 上述第二条规则有个例外，就是新增加一台storage server时，由已有的一台storage server将已有的所有数据（包括源头数据和备份数据）同步给该新增服务器。

storage server有7个状态，如下：

::

  # FDFS_STORAGE_STATUS_INIT      :初始化，尚未得到同步已有数据的源服务器
  # FDFS_STORAGE_STATUS_WAIT_SYNC :等待同步，已得到同步已有数据的源服务器
  # FDFS_STORAGE_STATUS_SYNCING   :同步中
  # FDFS_STORAGE_STATUS_DELETED   :已删除，该服务器从本组中摘除（注：本状态的功能尚未实现）
  # FDFS_STORAGE_STATUS_OFFLINE   :离线
  # FDFS_STORAGE_STATUS_ONLINE    :在线，尚不能提供服务
  # FDFS_STORAGE_STATUS_ACTIVE    :在线，可以提供服务

当storage server的状态为FDFS_STORAGE_STATUS_ONLINE时，当该storage server向tracker server发起一次heart beat时，tracker server将其状态更改为FDFS_STORAGE_STATUS_ACTIVE。

组内新增加一台storage server A时，由系统自动完成已有数据同步，处理逻辑如下：

1. storage server A连接tracker server，tracker server将storage server A的状态设置为FDFS_STORAGE_STATUS_INIT。storage server A询问追加同步的源服务器和追加同步截至时间点，如果该组内只有storage server A或该组内已成功上传的文件数为0，则没有数据需要同步，storage server A就可以提供在线服务，此时tracker将其状态设置为FDFS_STORAGE_STATUS_ONLINE，否则tracker server将其状态设置为FDFS_STORAGE_STATUS_WAIT_SYNC，进入第二步的处理；
2. 假设tracker server分配向storage server A同步已有数据的源storage server为B。同组的storage server和tracker server通讯得知新增了storage server A，将启动同步线程，并向tracker server询问向storage server A追加同步的源服务器和截至时间点。storage server B将把截至时间点之前的所有数据同步给storage server A；而其余的storage server从截至时间点之后进行正常同步，只把源头数据同步给storage server A。到了截至时间点之后，storage server B对storage server A的同步将由追加同步切换为正常同步，只同步源头数据；
3. storage server B向storage server A同步完所有数据，暂时没有数据要同步时，storage server B请求tracker server将storage server A的状态设置为FDFS_STORAGE_STATUS_ONLINE；
4. 当storage server A向tracker server发起heart beat时，tracker server将其状态更改为FDFS_STORAGE_STATUS_ACTIVE。
