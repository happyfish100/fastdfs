# FastDFS Dockerfile network (网络版本)

## 声明
其实并没什么区别 教程是在上一位huayanYu(小锅盖)和 Wiki的作者 的基础上进行了一些修改，本质上还是huayanYu(小锅盖) 和 Wiki 上的作者写的教程


## 目录介绍
### conf 
Dockerfile 所需要的一些配置文件
当然你也可以对这些文件进行一些修改  比如 storage.conf 里面的 bast_path 等相关

## 使用方法
需要注意的是 你需要在运行容器的时候制定宿主机的ip 用参数 FASTDFS_IPADDR 来指定



```
docker run -d -e FASTDFS_IPADDR=192.168.1.234 -p 8888:8888 -p 22122:22122 -p 23000:23000 -p 8011:80 --name test-fast 镜像id/镜像名称
```


## 后记
本质上 local 版本与  network 版本无区别




## Statement
In fact, there is no difference between the tutorials written by Huayan Yu and Wiki on the basis of their previous authors. In essence, they are also tutorials written by the authors of Huayan Yu and Wiki.

## Catalogue introduction
### conf 
Dockerfile Some configuration files needed
Of course, you can also make some modifications to these files, such as bast_path in storage. conf, etc.

## Usage method
Note that you need to specify the host IP when running the container with the parameter FASTDFS_IPADDR
Here's a sample docker run instruction
```
docker run -d -e FASTDFS_IPADDR=192.168.1.234 -p 8888:8888 -p 22122:22122 -p 23000:23000 -p 8011:80 --name test-fast 镜像id/镜像名称
```

## Epilogue
Essentially, there is no difference between the local version and the network version.

