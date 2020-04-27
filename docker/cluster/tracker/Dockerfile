# centos 7
FROM centos:7
# 添加配置文件

ADD startup.sh /home

# run
RUN yum install git gcc gcc-c++ make automake autoconf libtool pcre pcre-devel zlib zlib-devel openssl-devel wget vim -y \
  &&    cd /usr/local/src  \
  &&    git clone https://github.com/happyfish100/libfastcommon.git --depth 1        \
  &&    git clone https://github.com/happyfish100/fastdfs.git --depth 1    \
  &&    mkdir /home/dfs   \
  &&    cd /usr/local/src/  \
  &&    cd libfastcommon/   \
  &&    ./make.sh && ./make.sh install \
  &&    cd /usr/local/src/fastdfs \
  &&    ./make.sh && ./make.sh install  \
  &&    chmod +x /home/startup.sh


# export config
VOLUME ["/etc/fdfs","/home/dfs"]
ENTRYPOINT ["/home/startup.sh"]
