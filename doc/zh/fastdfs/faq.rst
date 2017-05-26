FAQ
============

定位问题首先要看日志文件。出现问题时，先检查返回的错误号和错误信息。然后查看服务器端日志，相信可以定位到问题所在。

1. FastDFS适用的场景以及不适用的场景？
   FastDFS是为互联网应用量身定做的一套分布式文件存储系统，非常适合用来存储用户图片、视频、文档等文件。对于互联网应用，和其他分布式文件系统相比，优势非常明显。
   具体情况大家可以看相关的介绍文档，包括FastDFS介绍PPT等等。
   出于简洁考虑，FastDFS没有对文件做分块存储，因此不太适合分布式计算场景。

2. FastDFS需要的编译和运行环境是怎样的？

   FastDFS Server仅支持unix系统，在Linux和FreeBSD测试通过。在Solaris系统下网络通信方面有些问题。

   编译需要的其他库文件有pthread，V5.0以前的版本依赖libevent；V5.0以后，不再依赖libevent。

   v5.04开始依赖libfastcommon，github地址：https://github.com/happyfish100/libfastcommon

   v5版本从v5.05开始才是稳定版本，请使用v5版本的同学尽快升级到v5.05或更新的版本，建议升级到v5.08。

   pthread使用系统自带的即可。

   对libevent的版本要求为1.4.x，建议使用最新的stable版本，如1.4.14b。

   注意，千万不要使用libevent 2.0非stable版本。

   测试了一下，libevent 2.0.10是可以正常工作的。

   在64位系统下，可能需要自己在/usr/lib64下创建libevent.so的符号链接。比如：

   ln -s /usr/lib/libevent.so /usr/lib64/libevent.so

   在ubuntu 11及后续版本，可能会出现找不到动态库pthread库，解决方法参见：http://bbs.chinaunix.net/thread-2324388-1-2.html

   若出现libfastcommon版本不匹配问题，请执行如下命令：/bin/rm -rf /usr/local/lib/libfastcommon.so /usr/local/include/fastcommon

3. 有人在生产环境中使用FastDFS吗？

   答案是肯定的。据我所知，截止2012年底至少有25家公司在使用FastDFS，其中有好几家是做网盘的公司。
   其中存储量最大的一家，集群中存储group数有400个，存储服务器超过800台，存储容量达到6PB，文件数超过1亿，Group持续增长中。。。
   以下是使用FastDFS的用户列表：

   #. 某大型网盘（因对方要求对公司名保密，就不提供名字了。有400个group，存储容量达到了6PB，文件数超过1亿）
   #. UC （http://www.uc.cn/，存储容量超过10TB）
   #. 支付宝（http://www.alipay.com/）
   #. 京东商城（http://www.360buy.com/）
   #. 淘淘搜（http://www.taotaosou.com/）
   #. 飞信（http://feixin.1008**/）
   #. 赶集网（http://www.ganji.com/）
   #. 淘米网（http://www.61.com/）
   #. 迅雷（http://www.xunlei.com/）
   #. 蚂蜂窝（http://www.mafengwo.cn/）
   #. 丫丫网（http://www.iyaya.com/）
   #. 虹网（http://3g.ahong.com）
   #. 5173（http://www.5173.com/）
   #. 华夏原创网（http://www.yuanchuang.com/）
   #. 华师京城教育云平台（http://www.hsjdy.com.cn/）
   #. 视友网（http://www.cuctv.com/）
   #. 搜道网（http://www.sodao.com/）
   #. 58同城（http://www.58.com/）
   #. 商务联盟网（http://www.biz72.com/）
   #. 中青网（http://www.youth.cn/）
   #. 缤丽网 （http://www.binliy.com/）
   #. 飞视云视频（http://www.freeovp.com/）
   #. 梦芭莎（http://www.moonbasa.com/）
   #. 活动帮（http://www.eventsboom.com）
   #. 51CTO（http://www.51cto.com/）
   #. 搜房网（http://www.soufun.com/）

4. 启动storage server时，一直处于僵死状态。

   A：启动storage server，storage将连接tracker server，如果连不上，将一直重试。直到连接成功，启动才算真正完成。

   出现这样情况，请检查连接不上tracker server的原因。

   友情提示：从V2.03以后，多tracker server在启动时会做时间上的检测，判断是否需要从别的tracker server同步4个系统文件。

   触发时机是第一个storage server连接上tracker server后，并发起join请求。

   如果集群中有2台tracker server，而其中一台tracker没有启动，可能会导致storage server一直处于僵死状态。

   注：这个问题v5.07解决了。

5. 执行fdfs_test或fdfs_test1上传文件时，服务器返回错误号2

   错误号表示没有ACTIVE状态的storage server。可以执行fdfs_monitor查看服务器状态。

6. 如何让server进程退出运行？

   直接kill即可让server进程正常退出，可以使用killall命令，例如： ::

       killall fdfs_trackerd
       killall fdfs_storaged

   也可以使用如下命令： ::

       /usr/bin/fdfs_trackerd /etc/fdfs/tracker.conf stop
       /usr/bin/fdfs_storaged /etc/fdfs/storage.conf stop

   千万不要使用-9参数强杀，否则可能会导致binlog数据丢失的问题。

7. 如何重启server进程？

   直接使用： ::

       /usr/bin/fdfs_trackerd /etc/fdfs/tracker.conf restart
       /usr/bin/fdfs_storaged /etc/fdfs/storage.conf restart

8. 跨运营商通信异常问题

   比如电信和网通机房相互通信，可能会存在异常，有两种表现：

   #. 不能建立连接，这个比较直接，肯定是网络连接的问题
   #. 可以正常建立连接，但接收和发送数据失败，这个问题比较隐蔽，正常网络环境下，不应该出现此类问题。

   还有人碰到过从一个方向建立连接可以正常通信，但从另外一个方向就不能正常通信的情况。
   解决办法：

     尝试将服务端口改小，建议将端口修改为1024以下。比如将storage服务端口由23000修改为873等，也可以试试修改为8080
     如果问题还不能解决，请联系你的网络（机房）服务商。

9. fdfs_test和fdfs_test1是做什么用的？

   这两个是FastDFS自带的测试程序，会对一个文件上传两次，分别作为主文件和从文件。返回的文件ID也是两个。
   并且会上传文件附加属性，storage server上会生成4个文件。
   这两个程序仅用于测试目的，请不要用作实际用途。

   V2.05提供了比较正式的三个小工具：

      * 上传文件：/usr/bin/fdfs_upload_file  <config_file> <local_filename>
      * 下载文件：/usr/bin/fdfs_download_file <config_file> <file_id> [local_filename]
      * 删除文件：/usr/bin/fdfs_delete_file <config_file> <file_id>

10. 什么是主从文件？

    主从文件是指文件ID有关联的文件，一个主文件可以对应多个从文件。

    ::

       主文件ID = 主文件名 + 主文件扩展名
       从文件ID = 主文件名 + 从文件后缀名 + 从文件扩展名

    使用主从文件的一个典型例子：以图片为例，主文件为原始图片，从文件为该图片的一张或多张缩略图。
    FastDFS中的主从文件只是在文件ID上有联系。FastDFS server端没有记录主从文件对应关系，因此删除主文件，FastDFS不会自动删除从文件。
    删除主文件后，从文件的级联删除，需要由应用端来实现。
    主文件及其从文件均存放到同一个group中。

    主从文件的生成顺序：

       #. 先上传主文件（如原文件），得到主文件ID
       #. 然后上传从文件（如缩略图），指定主文件ID和从文件后缀名（当然还可以同时指定从文件扩展名），得到从文件ID。


11. 如何删除无效的storage server？
    可以使用fdfs_monitor来删除。命令行如下：

    ::

       /usr/bin/fdfs_monitor <config_filename> delete <group_name> <storage_id>

    例如：

    ::

       /usr/bin/fdfs_monitor /etc/fdfs/client.conf delete group1 192.168.0.100

    注意：如果被删除的storage server的状态是ACTIVE，也就是该storage server还在线上服务的情况下，是无法删除掉的。

         storage_id参数：如果使用默认的ip方式，填写storage server IP地址，否则使用对应的server id。

12. FastDFS扩展模块升级到V1.06及以上版本的注意事项

    apache和nginx扩展模块版本v1.06及以上版本，需要在配置文件/etc/fdfs/fastdfs_mod.conf中设置storage server的存储路径信息。

    一个示例如下所示：

    ::

       store_path_count=1
       store_path0=/home/yuqing/fastdfs
       #store_path_count和store_path
       #均需要正确设置，必须和storage.conf中的相应配置完全一致，否则将导致文件不能正确下载！

13. nginx和apache扩展模块与FastDFS server版本对应关系

    扩展模块1.05：  针对FastDFs server v2.x，要求server版本大于等于v2.09

    扩展模块1.07及以上版本：  针对FastDFs server v3.x

    具体的版本匹配情况，参阅扩展模块源码下的HISTORY文件

14. FastDFS有QQ技术交流群吗？

    有的。群号：164684842，欢迎大家加入交流。

15. 上传文件失败，返回错误码28，这是怎么回事？

    返回错误码28，表示磁盘空间不足。注意FastDFS中有预留空间的概念，在tracker.conf中设置，配置项为：reserved_storage_space，缺省值为4GB，即预留4GB的空间。

    请酌情设置reserved_storage_space这个参数，比如可以设置为磁盘总空间的20%左右。

16. fdfs_trackerd或者fdfs_storaged的日志中出现：malloc task buff failed字样的错误，这是怎么回事？

    出现此类信息表示已经达到最大连接数。server端支持的最大连接数可以通过max_connections这个参数来设置。

    出现这样的问题，需要排查一下是否客户端使用不当导致的，比如客户端没有及时关闭无用的连接。

17. FastDFS的文件ID中可以反解出哪些字段？

    文件ID中除了包含group name和存储路径外，文件名中可以反解出如下几个字段：

    #. 文件创建时间（unix时间戳，32位整数）
    #. 文件大小
    #. 上传到的源storage server IP地址（32位整数）
    #. 文件crc32校验码
    #. 随机数（这个字段用来避免文件重名）

18. 为什么生成的token验证无法通过？

    出现这样的问题，请进行如下两项检查：

    #. 确认调用token生成函数，传递的文件ID中没有包含group name。传递的文件ID格式形如：M00/00/1B/wKgnVE84utyOG9hEAAATz5-S0SI99.java
    #. 确认服务器时间基本是一致的，注意服务器时间不能相差太多，不要相差到分钟级别。

19. 最新程序包的下载地址是什么？

    因google code不支持上传程序包，最新的程序包可以在sourceforge上下载，下载地址：https://sourceforge.net/projects/fastdfs/files/

20. FastDFS支持断点续传吗？

    可以支持。先上传appender类型的文件，然后使用apend函数。