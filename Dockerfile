#参考https://github.com/happyfish100/fastdfs/wiki
#nginx相关模块根据需要可以删掉  依赖pcre pcre-devel zlib zlib-devel openssl-devel
FROM centos:7                                                                                                                                                                                                                          

MAINTAINER liukunpeng<943243434@qq.com> 
#设置时间为中国时区
RUN \cp -f /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
#更新
RUN yum -y update

#编译环境
RUN yum install wget git gcc gcc-c++ make automake autoconf libtool pcre pcre-devel zlib zlib-devel openssl-devel -y

#创建跟踪服务器数据目录
RUN mkdir -p /fastdfs/tracker
#创建存储服务器数据目录
RUN mkdir -p /fastdfs/storage

#切换到安装目录#安装libfatscommon
WORKDIR /usr/local/src 
RUN git clone https://github.com/happyfish100/libfastcommon.git --depth 1
WORKDIR /usr/local/src/libfastcommon/
RUN ./make.sh && ./make.sh install

#切换到安装目录#安装FastDFS
WORKDIR /usr/local/src 
RUN git clone https://github.com/happyfish100/fastdfs.git --depth 1
WORKDIR /usr/local/src/fastdfs/
RUN ./make.sh && ./make.sh install
#配置文件准备  
#RUN cp /etc/fdfs/tracker.conf.sample /etc/fdfs/tracker.conf
#RUN cp /etc/fdfs/storage.conf.sample /etc/fdfs/storage.conf
#RUN cp /etc/fdfs/client.conf.sample /etc/fdfs/client.conf #客户端文件，测试用
#RUN cp /usr/local/src/fastdfs/conf/http.conf /etc/fdfs/ #供nginx访问使用
#RUN cp /usr/local/src/fastdfs/conf/mime.types /etc/fdfs/ #供nginx访问使用


#切换到安装目录#安装fastdfs-nginx-module
WORKDIR /usr/local/src
RUN git clone https://github.com/happyfish100/fastdfs-nginx-module.git --depth 1
RUN cp /usr/local/src/fastdfs-nginx-module/src/mod_fastdfs.conf /etc/fdfs

#切换到安装目录#安装安装nginx
WORKDIR /usr/local/src
RUN wget http://nginx.org/download/nginx-1.13.9.tar.gz
RUN tar -zxvf nginx-1.13.9.tar.gz
WORKDIR /usr/local/src/nginx-1.13.9
#添加fastdfs-nginx-module模块
RUN ./configure --add-module=/usr/local/src/fastdfs-nginx-module/src/
RUN make && make install
#tracker配置服务端口默认22122#存储日志和数ca据的根目录
RUN sed 's/^base_path.*/base_path=\/fastdfs\/tracker/g' /etc/fdfs/tracker.conf.sample > /etc/fdfs/tracker.conf
RUN fdfs_trackerd /etc/fdfs/tracker.conf start

#storage配置服务端口默认23000 # 数据和日志文件存储根目录# 第一个存储目录# tracker服务器IP和端口
RUN sed 's/^base_path.*/base_path=\/fastdfs\/storage/g' /etc/fdfs/storage.conf.sample > /etc/fdfs/storage.conf
RUN sed 's/^store_path0.*/store_path0=\/fastdfs\/storage/g' /etc/fdfs/storage.conf > /etc/fdfs/storage.conf.tmp
RUN cat /etc/fdfs/storage.conf.tmp > /etc/fdfs/storage.conf
RUN sed 's/^tracker_server.*/tracker_server=127.0.0.1:22122/g' /etc/fdfs/storage.conf > /etc/fdfs/storage.conf.tmp
RUN cat /etc/fdfs/storage.conf.tmp > /etc/fdfs/storage.conf
RUN fdfs_storaged /etc/fdfs/storage.conf start

WORKDIR /usr/local/src/
EXPOSE 22122 23000

#ENTRYPOINT tail -f /fastdfs/storage/logs/storaged.log
#ENTRYPOINT tail -f /fastdfs/tracker/logs/trackerd.log 
ENTRYPOINT tail -f /dev/null
#执行dockerfile
#docker build -t="lkp/fastdfs-storaged:0.9" .
#docker build -t="lkp/fastdfs-trackerd:0.9" .

