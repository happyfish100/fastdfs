Copyright (C) 2008 Happy Fish / YuQing

FastDFS may be copied only under the terms of the GNU General
Public License V3, which may be found in the FastDFS source kit.
Please visit the FastDFS Home Page for more detail.
English language: http://english.csource.org/
Chinese language: http://www.csource.org/


FastDFS is an open source high performance distributed file system. It's major 
functions include: file storing, file syncing and file accessing (file uploading
and file downloading), and it can resolve the high capacity and load balancing 
problem. FastDFS should meet the requirement of the website whose service based
on files such as photo sharing site and video sharing site.

FastDFS has two roles: tracker and storage. The tracker takes charge of 
scheduling and load balancing for file access. The storage store files and it's
function is file management including: file storing, file syncing, providing file
access interface. It also manage the meta data which are attributes representing
as key value pair of the file. For example: width=1024, the key is "width" and
the value is "1024".

The tracker and storage contain one or more servers. The servers in the tracker
or storage cluster can be added to or removed from the cluster by any time without
affecting the online services. The servers in the tracker cluster are peer to peer.

The storarge servers organizing by the file volume/group to obtain high capacity.
The storage system contains one or more volumes whose files are independent among
these volumes. The capacity of the whole storage system equals to the sum of all
volumes' capacity. A file volume contains one or more storage servers whose files
are same among these servers. The servers in a file volume backup each other, 
and all these servers are load balancing. When adding a storage server to a 
volume, files already existing in this volume are replicated to this new server
automatically, and when this replication done, system will switch this server
online to providing storage services.

When the whole storage capacity is insufficiency, you can add one or more 
volumes to expand the storage capacity. To do this, you need to add one or
more storage servers.

The identification of a file is composed of two parts: the volume name and
the file name.

Client test code use client library please refer to the directory: client/test.
