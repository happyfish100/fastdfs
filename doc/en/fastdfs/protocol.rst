Protocol
=======================

The communication protocol of FastDFS is TCP/IP, the package composes of header and body which may be empty.

header format
-----------------------

.. code-block:: ini

  @ TRACKER_PROTO_PKG_LEN_SIZE bytes package length
  @ 1 byte command
  @ 1 byte status

note::

   # TRACKER_PROTO_PKG_LEN_SIZE (8) bytes number buff is Big-Endian bytes


body format
-----------------------

1. common command
^^^^^^^^^^^^^^^^^^^^^^^

* FDFS_PROTO_CMD_QUIT

::

  # function: notify server connection will be closed
  # request body: none (no body part)
  # response: none (no header and no body)

* FDFS_PROTO_CMD_ACTIVE_TEST

::

  # function: active test
  # request body: none
  # response body: none

2. storage server to tracker server command
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* the reponse command is TRACKER_PROTO_CMD_STORAGE_RESP

* TRACKER_PROTO_CMD_STORAGE_JOIN

::

  # function: storage join to tracker
  # request body:
      @ FDFS_GROUP_NAME_MAX_LEN + 1 bytes: group name
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage http server port
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: path count
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: subdir count per path
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: upload priority
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: join time (join timestamp)
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: up time (start timestamp)
      @ FDFS_VERSION_SIZE bytes: storage server version
      @ FDFS_DOMAIN_NAME_MAX_SIZE bytes: domain name of the web server on the storage server
      @ 1 byte: init flag ( 1 for init done)
      @ 1 byte: storage server status
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: tracker server count excluding current tracker
  # response body:
      @ FDFS_IPADDR_SIZE bytes: sync source storage server ip address
  # memo: return all storage servers in the group only when storage servers changed or return none


* TRACKER_PROTO_CMD_STORAGE_BEAT

::

  # function: heart beat
  # request body: none or storage stat info
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total upload count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success upload count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total set metadata count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success set metadata count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total delete count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success delete count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total download count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success download count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total get metadata count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success get metadata count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total create link count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success create link count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total delete link count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success delete link count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: last source update timestamp
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: last sync update timestamp
       @TRACKER_PROTO_PKG_LEN_SIZE bytes:  last synced timestamp
       @TRACKER_PROTO_PKG_LEN_SIZE bytes:  last heart beat timestamp
  # response body: n * (1 + FDFS_IPADDR_SIZE) bytes, n >= 0. One storage entry format:
      @ 1 byte: storage server status
      @ FDFS_IPADDR_SIZE bytes: storage server ip address
  # memo: storage server sync it's stat info to tracker server only when storage stat info changed


* TRACKER_PROTO_CMD_STORAGE_REPORT

::

  # function: report disk usage
  # request body 1 or more than 1 following entries:
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total space in MB
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: free space in MB
  # response body: same to command TRACKER_PROTO_CMD_STORAGE_BEAT


* TRACKER_PROTO_CMD_STORAGE_REPLICA_CHG

::

  # function: replica new storage servers which maybe not exist in the tracker server
  # request body: n * (1 + FDFS_IPADDR_SIZE) bytes, n >= 1. One storage entry format:
      @ 1 byte: storage server status
      @ FDFS_IPADDR_SIZE bytes: storage server ip address
  # response body: none


* TRACKER_PROTO_CMD_STORAGE_SYNC_SRC_REQ

::

  # function: source storage require sync. when add a new storage server, the existed storage servers in the same group will ask the tracker server to tell the source storage server which will sync old data to it
  # request body:
      @ FDFS_GROUP_NAME_MAX_LEN: group name
      @ FDFS_IPADDR_SIZE bytes: dest storage server (new storage server) ip address
  # response body: none or
     @ FDFS_IPADDR_SIZE bytes: source storage server ip address
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: sync until timestamp
  # memo: if the dest storage server not do need sync from one of storage servers in the group, the response body is emtpy


* TRACKER_PROTO_CMD_STORAGE_SYNC_DEST_REQ

::

  # function: dest storage server (new storage server) require sync
  # request body: none
  # response body: none or
     @ FDFS_IPADDR_SIZE bytes: source storage server ip address
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: sync until timestamp
  # memo: if the dest storage server not do need sync from one of storage servers in the group, the response body is emtpy


* TRACKER_PROTO_CMD_STORAGE_SYNC_NOTIFY

::

  # function: new storage server sync notify
  # request body:
     @ FDFS_IPADDR_SIZE bytes: source storage server ip address
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: sync until timestamp
  # response body: same to command TRACKER_PROTO_CMD_STORAGE_BEAT


3. client to tracker server command
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* the reponse command of following 2 commands is TRACKER_PROTO_CMD_SERVER_RESP

* TRACKER_PROTO_CMD_SERVER_LIST_GROUP

::

  # function: list all groups
  # request body: none
  # response body: n group entries, n >= 0, the format of each entry:
     @ FDFS_GROUP_NAME_MAX_LEN+1 bytes: group name
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: free disk storage in MB
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server count
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server http port
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: active server count
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: current write server index
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: store path count on storage server
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: subdir count per path on storage server


* TRACKER_PROTO_CMD_SERVER_LIST_STORAGE

::

  # function: list storage servers of a group
  # request body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: the group name to query
  # response body: n storage entries, n >= 0, the format of each entry:
       @ 1 byte: status
       @ FDFS_IPADDR_SIZE bytes: ip address
       @ FDFS_DOMAIN_NAME_MAX_SIZE  bytes : domain name of the web server
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: source storage server ip address
       @ FDFS_VERSION_SIZE bytes: storage server version
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: join time (join in timestamp)
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: up time (start timestamp)
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total space in MB
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: free space in MB
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: upload priority
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: store path count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: subdir count per path
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: current write path[
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage http port
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total upload count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success upload count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total set metadata count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success set metadata count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total delete count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success delete count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total download count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success download count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total get metadata count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success get metadata count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total create link count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success create link count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: total delete link count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: success delete link count
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: last source update timestamp
       @ TRACKER_PROTO_PKG_LEN_SIZE bytes: last sync update timestamp
       @TRACKER_PROTO_PKG_LEN_SIZE bytes:  last synced timestamp
       @TRACKER_PROTO_PKG_LEN_SIZE bytes:  last heart beat timestamp

* the reponse command of following 2 commands is TRACKER_PROTO_CMD_SERVICE_RESP

* TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE

::

  # function: query which storage server to store file
  # request body: none
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ FDFS_IPADDR_SIZE - 1 bytes: storage server ip address
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port
     @1 byte: store path index on the storage server


* TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL

::

  # function: query which storage server to store file
  # request body: none
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ FDFS_IPADDR_SIZE - 1 bytes: storage server ip address (* multi)
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port (*multi)
     @1 byte: store path index on the storage server


* TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ONE

::

  # function: query which storage server to store file
  # request body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ FDFS_IPADDR_SIZE - 1 bytes: storage server ip address
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port
     @1 byte: store path index on the storage server


* TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ALL

::

  # function: query which storage server to store file
  # request body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ FDFS_IPADDR_SIZE - 1 bytes: storage server ip address  (* multi)
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port   (* multi)
     @1 byte: store path index on the storage server


* TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH

::

  # function: query which storage server to download the file
  # request body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ filename bytes: filename
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ FDFS_IPADDR_SIZE - 1 bytes: storage server ip address
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port

* TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ALL

::

  # function: query all storage servers to download the file
  # request body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ filename bytes: filename
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ FDFS_IPADDR_SIZE - 1 bytes: storage server ip address
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port
     @ n * (FDFS_IPADDR_SIZE - 1) bytes:  storage server ip addresses, n can be 0

* TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE

::

  # function: query which storage server to download the file
  # request body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ filename bytes: filename
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ FDFS_IPADDR_SIZE - 1 bytes: storage server ip address
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: storage server port


4. storage server to storage server command
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* the reponse command is STORAGE_PROTO_CMD_RESP

* STORAGE_PROTO_CMD_SYNC_CREATE_FILE

::

  # function: sync new created file
  # request body:
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: filename bytes
     @ TRACKER_PROTO_PKG_LEN_SIZE bytes: file size/bytes
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ filename bytes : filename
     @ file size bytes: file content
  # response body: none


* STORAGE_PROTO_CMD_SYNC_DELETE_FILE

::

  # function: sync deleted file
  # request body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ filename bytes: filename
  # response body: none


* STORAGE_PROTO_CMD_SYNC_UPDATE_FILE

::

  # function: sync updated file
  # request body: same to command STORAGE_PROTO_CMD_SYNC_CREATE_FILE
  # respose body: none


5. client to storage server command
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* the reponse command is STORAGE_PROTO_CMD_RESP

* STORAGE_PROTO_CMD_UPLOAD_FILE

::

  # function: upload file to storage server
  # request body:
      @ 1 byte: store path index on the storage server
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: file size
      @ FDFS_FILE_EXT_NAME_MAX_LEN bytes: file ext name, do not include dot (.)
      @ file size bytes: file content
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ filename bytes: filename

  * STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE
  # function: upload slave file to storage server
  # request body:
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: master filename length
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: file size
      @ FDFS_FILE_PREFIX_MAX_LEN bytes: filename prefix
      @ FDFS_FILE_EXT_NAME_MAX_LEN bytes: file ext name, do not include dot (.)
      @ master filename bytes: master filename
      @ file size bytes: file content
  # response body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ filename bytes: filename

* STORAGE_PROTO_CMD_DELETE_FILE

::

  # function: delete file from storage server
  # request body:
     @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
     @ filename bytes: filename
  # response body: none


* STORAGE_PROTO_CMD_SET_METADATA

::

  # function: delete file from storage server
  # request body:
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: filename length
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: meta data size
      @ 1 bytes: operation flag,
           'O' for overwrite all old metadata
           'M' for merge, insert when the meta item not exist, otherwise update it
      @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
      @ filename bytes: filename
      @ meta data bytes: each meta data seperated by \x01,
                         name and value seperated by \x02
  # response body: none


* STORAGE_PROTO_CMD_DOWNLOAD_FILE

::

  # function: download/fetch file from storage server
  # request body:
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: file offset
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: download file bytes
      @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
      @ filename bytes: filename
  # response body:
      @ file content


* STORAGE_PROTO_CMD_GET_METADATA

::

  # function: get metat data from storage server
  # request body:
      @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
      @ filename bytes: filename
  # response body
      @ meta data buff, each meta data seperated by \x01, name and value seperated by \x02


* STORAGE_PROTO_CMD_QUERY_FILE_INFO

::

  # function: query file info from storage server
  # request body:
      @ FDFS_GROUP_NAME_MAX_LEN bytes: group name
      @ filename bytes: filename
  # response body:
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: file size
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: file create timestamp
      @ TRACKER_PROTO_PKG_LEN_SIZE bytes: file CRC32 signature