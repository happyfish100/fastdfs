# 选择系统镜像作为基础镜像，可以使用超小的Linux镜像alpine
#FROM centos:7
FROM alpine:3.16

LABEL MAINTAINER  liyanjing 284223249@qq.com

# 注意
#    v6.0.9 依赖libfastcommon和libserverframe, v6.0.8及以下依赖libevent和libfastcommon两个库，其中libfastcommon是 FastDFS 官方提供的
#    v6.0.9 适配fastdfs-nginx-module-1.23，v6.0.8及以下是fastdfs-nginx-module-1.22

# 0.安装包位置，fdfs的基本目录和存储目录
ENV INSTALL_PATH=/usr/local/src \
  LIBFASTCOMMON_VERSION="1.0.60" \
  LIBSERVERFRAME_VERSION="1.1.19" \
  FASTDFS_VERSION="V6.09" \
  FASTDFS_NGINX_MODULE_VERSION="1.23" \
  NGINX_VERSION="1.22.0" \
  TENGINE_VERSION="2.3.3" 

# 0.change the system source for installing libs
RUN echo "http://mirrors.aliyun.com/alpine/v3.16/main" > /etc/apk/repositories \
  && echo "http://mirrors.aliyun.com/alpine/v3.16/community" >> /etc/apk/repositories

# 1.复制安装包
ADD soft ${INSTALL_PATH}

# 2.环境安装
# - 创建fdfs的存储目录
# - 安装依赖
# - 安装libfastcommon
# - 安装fastdfs
# - 安装nginx,设置nginx和fastdfs联合环境
#Run yum -y install -y gcc gcc-c++ libevent libevent-devel make automake autoconf libtool perl pcre pcre-devel zlib zlib-devel openssl openssl-devel zip unzip net-tools wget vim lsof  \
RUN  apk update  && apk add --no-cache --virtual .build-deps bash autoconf gcc libc-dev make pcre-dev zlib-dev linux-headers gnupg libxslt-dev gd-dev geoip-dev wget \
  &&  cd ${INSTALL_PATH}  \
  &&  tar -zxf libfastcommon-${LIBFASTCOMMON_VERSION}.tar.gz \
  &&  tar -zxf libserverframe-${LIBSERVERFRAME_VERSION}.tar.gz \
  &&  tar -zxf fastdfs-${FASTDFS_VERSION}.tar.gz \
  &&  tar -zxf fastdfs-nginx-module-${FASTDFS_NGINX_MODULE_VERSION}.tar.gz \
  &&  tar -zxf nginx-${NGINX_VERSION}.tar.gz \
      \  
  &&  cd ${INSTALL_PATH}/libfastcommon-${LIBFASTCOMMON_VERSION}/ \
  &&  ./make.sh \
  &&  ./make.sh install \
  &&  cd ${INSTALL_PATH}/libserverframe-${LIBSERVERFRAME_VERSION}/ \
  &&  ./make.sh \
  &&  ./make.sh install \
  &&  cd ${INSTALL_PATH}/fastdfs-${FASTDFS_VERSION}/ \
  &&  ./make.sh \
  &&  ./make.sh install \
      \  
  &&  cd ${INSTALL_PATH}/nginx-${NGINX_VERSION}/ \
  &&  ./configure --prefix=/usr/local/nginx --pid-path=/var/run/nginx/nginx.pid --with-http_stub_status_module --with-http_gzip_static_module --with-http_realip_module --with-http_sub_module --with-stream=dynamic \
      --add-module=${INSTALL_PATH}/fastdfs-nginx-module-${FASTDFS_NGINX_MODULE_VERSION}/src/ \
  &&  make \
  &&  make install \
      \  
  &&  rm -rf ${INSTALL_PATH}/* \
  &&  apk del .build-deps gcc libc-dev make linux-headers gnupg libxslt-dev gd-dev geoip-dev wget
  
# 3.添加配置文件，目标路径以/结尾，docker会把它当作目录，不存在时，会自动创建
COPY conf/*.* /etc/fdfs/
COPY nginx_conf/nginx.conf /usr/local/nginx/conf/
COPY nginx_conf.d/*.conf /usr/local/nginx/conf.d/
COPY start.sh /


ENV TZ=Asia/Shanghai

# 4.更改启动脚本执行权限，设置时区为中国时间
RUN chmod u+x /start.sh  \
 && apk add --no-cache bash pcre-dev zlib-dev \
 \
 && apk add -U tzdata \
 && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone \
 && apk del tzdata && rm -rf /var/cache/apk/*

EXPOSE 22122 23000 9088

WORKDIR /

# 镜像启动
ENTRYPOINT ["/bin/bash","/start.sh"]
