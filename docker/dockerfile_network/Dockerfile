# centos 7
FROM centos:7
# 添加配置文件
ADD conf/client.conf /etc/fdfs/
ADD conf/http.conf /etc/fdfs/
ADD conf/mime.types /etc/fdfs/
ADD conf/storage.conf /etc/fdfs/
ADD conf/tracker.conf /etc/fdfs/
ADD fastdfs.sh /home
ADD conf/nginx.conf /etc/fdfs/
ADD conf/mod_fastdfs.conf /etc/fdfs

# run
# install packages
RUN yum install git gcc gcc-c++ make automake autoconf libtool pcre pcre-devel zlib zlib-devel openssl-devel wget vim -y
# git clone libfastcommon / libserverframe / fastdfs / fastdfs-nginx-module
RUN cd /usr/local/src \
  && git clone https://gitee.com/fastdfs100/libfastcommon.git \
  && git clone https://gitee.com/fastdfs100/libserverframe.git \
  && git clone https://gitee.com/fastdfs100/fastdfs.git \
  && git clone https://gitee.com/fastdfs100/fastdfs-nginx-module.git \
  && pwd && ls
# build libfastcommon / libserverframe / fastdfs
RUN mkdir /home/dfs \
  && cd /usr/local/src  \
  && pwd && ls \
  && cd libfastcommon/   \
  && ./make.sh && ./make.sh install  \
  && cd ../  \
  && cd libserverframe/   \
  && ./make.sh && ./make.sh install  \
  && cd ../  \
  && cd fastdfs/   \
  && ./make.sh && ./make.sh install
# download nginx and build with fastdfs-nginx-module
# 推荐 NGINX 版本:
# NGINX_VERSION=1.16.1
# NGINX_VERSION=1.17.10
# NGINX_VERSION=1.18.0
# NGINX_VERSION=1.19.10
# NGINX_VERSION=1.20.2
# NGINX_VERSION=1.21.6
# NGINX_VERSION=1.22.1
# NGINX_VERSION=1.23.3
# 可在 docker build 命令中指定使用的 nginx 版本, 例如:
# docker build --build-arg NGINX_VERSION="1.16.1" -t happyfish100/fastdfs:latest -t happyfish100/fastdfs:6.09.01 .
# docker build --build-arg NGINX_VERSION="1.19.10" -t happyfish100/fastdfs:latest -t happyfish100/fastdfs:6.09.02 .
# docker build --build-arg NGINX_VERSION="1.23.3" -t happyfish100/fastdfs:latest -t happyfish100/fastdfs:6.09.03 .
ARG NGINX_VERSION=1.16.1
RUN cd /usr/local/src \
  && NGINX_PACKAGE=nginx-${NGINX_VERSION} \
  && NGINX_FILE=${NGINX_PACKAGE}.tar.gz \
  && wget http://nginx.org/download/${NGINX_FILE} \ 
  && tar -zxvf ${NGINX_FILE} \
  && pwd && ls \
  && cd /usr/local/src \
  && cd ${NGINX_PACKAGE}/  \
  && ./configure --add-module=/usr/local/src/fastdfs-nginx-module/src/   \
  && make && make install  \
  && chmod +x /home/fastdfs.sh

# 原来的 RUN 语句太复杂, 不利于 docker build 时使用多阶段构建缓存
# RUN yum install git gcc gcc-c++ make automake autoconf libtool pcre pcre-devel zlib zlib-devel openssl-devel wget vim -y \
#   &&    NGINX_VERSION=1.19.9 \
#   &&    NGINX_PACKAGE=nginx-${NGINX_VERSION} \
#   &&    NGINX_FILE=${NGINX_PACKAGE}.tar.gz \
#   &&    cd /usr/local/src  \
#   &&    git clone https://gitee.com/fastdfs100/libfastcommon.git \
#   &&    git clone https://gitee.com/fastdfs100/libserverframe.git \
#   &&    git clone https://gitee.com/fastdfs100/fastdfs.git \
#   &&    git clone https://gitee.com/fastdfs100/fastdfs-nginx-module.git \
#   &&    wget http://nginx.org/download/${NGINX_FILE}    \
#   &&    tar -zxvf ${NGINX_FILE}    \
#   &&    mkdir /home/dfs   \
#   &&    cd /usr/local/src/  \
#   &&    cd libfastcommon/   \
#   &&    ./make.sh && ./make.sh install  \
#   &&    cd ../  \
#   &&    cd libserverframe/   \
#   &&    ./make.sh && ./make.sh install  \
#   &&    cd ../  \
#   &&    cd fastdfs/   \
#   &&    ./make.sh && ./make.sh install  \
#   &&    cd ../  \
#   &&    cd ${NGINX_PACKAGE}/  \
#   &&    ./configure --add-module=/usr/local/src/fastdfs-nginx-module/src/   \
#   &&    make && make install  \
#   &&    chmod +x /home/fastdfs.sh

RUN ln -s /usr/local/src/fastdfs/init.d/fdfs_trackerd /etc/init.d/fdfs_trackerd \
  && ln -s /usr/local/src/fastdfs/init.d/fdfs_storaged /etc/init.d/fdfs_storaged 

# export config
VOLUME /etc/fdfs

EXPOSE 22122 23000 8888 80
ENTRYPOINT ["/home/fastdfs.sh"]
