# FastDFS-Storage Docker Cluster

## 声明
本 Docker 镜像 基于Fstdfs 并且参考了 huayanYu(小锅盖)和Wiki的作者 

## 概述
适用于集群分布式部署使用
开箱即用

## 构建镜像
```
docker build --tag storage .
```

## 简单使用
```
docker run -d --net host --name some-storage -e SERVER_PORT=24001 -e TRSERVER=127.0.0.1:22122 -e GROUP_NAME=group1 storage
```

## Tracker 集群的时候
使用 ,分割 填写多个 Tracker 地址即可 
```
docker run -d --net host --name storage-1 -e SERVER_PORT=24001 -e TR_SERVER=127.0.0.1:22122,127.0.0.1:22122 -e GROUP_NAME=group1 storage
```

## 环境变量
<ul>
    <li>TR_SERVER  Tracker地址 <font color="red">必填</font></li>
    <li>SERVER_PORT  Storage 服务端口 集群的情况下  不建议 使用 -p 来进行映射 请尽可能的使用 --net host 除非你能明确知道为什么不建议使用 -p 映射  <font color="red">必填</font></li>
    <li>GROUP_NAME  组名称 默认值 group1 非必填</li>
</ul>

若想进行更多的配置
请映射数据卷

## 数据卷
<ul>
    <li>/etc/fdfs  配置文件目录</li>
    <li>/home/dfs  数据文件以及日志</li>
</ul>