# FastDFS-Tracker Docker Cluster

## 声明
本 Docker 镜像 基于Fstdfs 并且参考了 huayanYu(小锅盖)和Wiki的作者 

## 概述
适用于集群分布式部署使用
开箱即用

## 构建镜像
```
docker build --tag tracker .
```

## 简单使用
```
docker run -d --name some-tracker -e SERVER_PORT=22122 -p 22122:22122 tracker
```

## 对端口 以及Storage集群的访问方式进行调整

```
docker run -d --name some-tracker -e SERVER_PORT=23122 -p 22122:23122 -e LOOKUP=0 tracker
```

## 环境变量

<ul>
    <li>SERVER_PORT  Tracker 服务端口</li>
    <li>LOOKUP  负载模式 默认值 0 非必填</li>
</ul>

LOOKUP 可选项
0 : round robin
1 : specify group
2 : load balance, select the max free space group to upload file

若想进行更多的配置
请映射数据卷

## 数据卷
<ul>
    <li>/etc/fdfs  配置文件目录</li>
    <li>/home/dfs  数据文件以及日志</li>
</ul>