
Copyright (C) 2008 Happy Fish / YuQing

FastDFS client php extension may be copied only under the terms of 
the Less GNU General Public License (LGPL).

Please visit the FastDFS Home Page for more detail.
Google code (English language): http://code.google.com/p/fastdfs/
Chinese language: http://www.csource.com/

In file fastdfs_client.ini, item fastdfs_client.tracker_group# point to
the FastDFS client config filename. Please read ../INSTALL file to know 
about how to config FastDFS client.

FastDFS client php extension compiled under PHP 5.2.x, Steps:
phpize
./configure
make
make install

#copy lib file to php extension directory, eg. /usr/lib/php/20060613/
cp modules/fastdfs_client.so  /usr/lib/php/20060613/

#copy fastdfs_client.ini to PHP etc directory, eg. /etc/php/
cp fastdfs_client.ini /etc/php/

#modify config file fastdfs_client.ini, such as:
vi /etc/php/fastdfs_client.ini

#run fastdfs_test.php
php fastdfs_test.php


FastDFS PHP functions:

string fastdfs_client_version()
return client library version


long fastdfs_get_last_error_no()
return last error no


string fastdfs_get_last_error_info()
return last error info


string fastdfs_http_gen_token(string remote_filename, int timestamp)
generate anti-steal token for HTTP download
parameters:
	remote_filename: the remote filename (do NOT including group name)
	timestamp: the timestamp (unix timestamp)
return token string for success, false for error


array fastdfs_get_file_info(string group_name, string filename)
get file info from the filename
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
return assoc array for success, false for error. 
	the assoc array including following elements:
		create_timestamp: the file create timestamp (unix timestamp)
		file_size: the file size (bytes)
		source_ip_addr: the source storage server ip address
	

array fastdfs_get_file_info1(string file_id)
get file info from the file id
parameters:
	file_id: the file id (including group name and filename) or remote filename
return assoc array for success, false for error. 
	the assoc array including following elements:
		create_timestamp: the file create timestamp (unix timestamp)
		file_size: the file size (bytes)
		source_ip_addr: the source storage server ip address
	

bool fastdfs_send_data(int sock, string buff)
parameters:
	sock: the unix socket description
	buff: the buff to send
return true for success, false for error


string fastdfs_gen_slave_filename(string master_filename, string prefix_name
                [, string file_ext_name])
generate slave filename by master filename, prefix name and file extension name
parameters:
	master_filename: the master filename / file id to generate 
			the slave filename
	prefix_name: the prefix name  to generate the slave filename
	file_ext_name: slave file extension name, can be null or emtpy 
			(do not including dot)
return slave filename string for success, false for error


boolean fastdfs_storage_file_exist(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
check file exist
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for exist, false for not exist


boolean fastdfs_storage_file_exist1(string file_id
	[, array tracker_server, array storage_server])
parameters:
	file_id: the file id of the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for exist, false for not exist


array fastdfs_storage_upload_by_filename(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload local file to storage server
parameters:
	local_filename: the local filename
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string fastdfs_storage_upload_by_filename1(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload local file to storage server
parameters:
	local_filename: the local filename
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error.


array fastdfs_storage_upload_by_filebuff(string file_buff
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file buff to storage server
parameters:
	file_buff: the file content
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string fastdfs_storage_upload_by_filebuff1(string file_buff
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file buff to storage server
parameters:
	file_buff: the file content
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


array fastdfs_storage_upload_by_callback(array callback_array
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file to storage server by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


array fastdfs_storage_upload_by_callback1(array callback_array
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file to storage server by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


array fastdfs_storage_upload_appender_by_filename(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload local file to storage server as appender file
parameters:
	local_filename: the local filename
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string fastdfs_storage_upload_appender_by_filename1(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload local file to storage server as appender file
parameters:
	local_filename: the local filename
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error.


array fastdfs_storage_upload_appender_by_filebuff(string file_buff
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file buff to storage server as appender file
parameters:
	file_buff: the file content
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string fastdfs_storage_upload_appender_by_filebuff1(string file_buff
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file buff to storage server as appender file
parameters:
	file_buff: the file content
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


array fastdfs_storage_upload_appender_by_callback(array callback_array
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file to storage server by callback as appender file
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string fastdfs_storage_upload_appender_by_callback1(array callback_array
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file to storage server by callback as appender file
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


boolean fastdfs_storage_append_by_filename(string local_filename, 
	string group_name, appender_filename
	[, array tracker_server, array storage_server])
append local file to the appender file of storage server
parameters:
	local_filename: the local filename
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



string fastdfs_storage_append_by_filename1(string local_filename, 
	string appender_file_id [, array tracker_server, array storage_server])
append local file to the appender file of storage server
parameters:
	local_filename: the local filename
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_append_by_filebuff(string file_buff, 
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
append file buff to the appender file of storage server
parameters:
	file_buff: the file content
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_append_by_filebuff1(string file_buff, 
	string appender_file_id [, array tracker_server, array storage_server])
append file buff to the appender file of storage server
parameters:
	file_buff: the file content
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_append_by_callback(array callback_array,
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
append file to the appender file of storage server by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_append_by_callback1(array callback_array,
	string appender_file_id [, array tracker_server, array storage_server])
append file buff to the appender file of storage server
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_modify_by_filename(string local_filename, 
	long file_offset, string group_name, appender_filename, 
	[array tracker_server, array storage_server])
modify appender file by local file
parameters:
	local_filename: the local filename
        file_offset: offset of appender file
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_modify_by_filename1(string local_filename, 
	long file_offset, string appender_file_id
        [, array tracker_server, array storage_server])
modify appender file by local file
parameters:
	local_filename: the local filename
        file_offset: offset of appender file
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_modify_by_filebuff(string file_buff, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
modify appender file by file buff
parameters:
	file_buff: the file content
        file_offset: offset of appender file
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_modify_by_filebuff1(string file_buff, 
	long file_offset, string appender_file_id
	[, array tracker_server, array storage_server])
modify appender file by file buff
parameters:
	file_buff: the file content
        file_offset: offset of appender file
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_modify_by_callback(array callback_array, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
modify appender file by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
        file_offset: offset of appender file
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_modify_by_callback1(array callback_array, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
modify appender file by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
        file_offset: offset of appender file
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_truncate_file(string group_name, 
	string appender_filename [, long truncated_file_size = 0, 
	array tracker_server, array storage_server])
truncate appender file to specify size
parameters:
	group_name: the the group name of appender file
	appender_filename: the appender filename
        truncated_file_size: truncate the file size to
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean fastdfs_storage_truncate_file1(string appender_file_id
	[, long truncated_file_size = 0, array tracker_server, 
	array storage_server])
truncate appender file to specify size
parameters:
	appender_file_id: the appender file id
        truncated_file_size: truncate the file size to
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


string/array fastdfs_storage_upload_slave_by_filename(string local_filename, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload local file to storage server (slave file mode)
parameters:
	file_buff: the file content
	group_name: the group name of the master file
	master_filename: the master filename to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string fastdfs_storage_upload_slave_by_filename1(string local_filename, 
	string master_file_id, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload local file to storage server (slave file mode)
parameters:
	local_filename: the local filename
	master_file_id: the master file id to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error.


array fastdfs_storage_upload_slave_by_filebuff(string file_buff, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload file buff to storage server (slave file mode)
parameters:
	file_buff: the file content
	group_name: the group name of the master file
	master_filename: the master filename to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string fastdfs_storage_upload_slave_by_filebuff1(string file_buff, 
	string master_file_id, string prefix_name
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload file buff to storage server (slave file mode)
parameters:
	file_buff: the file content
	master_file_id: the master file id to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


array fastdfs_storage_upload_slave_by_callback(array callback_array,
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload file to storage server by callback (slave file mode)
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	group_name: the group name of the master file
	master_filename: the master filename to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string fastdfs_storage_upload_slave_by_callback1(array callback_array,
	string master_file_id, string prefix_name
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload file to storage server by callback (slave file mode)
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	master_file_id: the master file id to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


boolean fastdfs_storage_delete_file(string group_name, string remote_filename 
	[, array tracker_server, array storage_server])
delete file from storage server
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean fastdfs_storage_delete_file1(string file_id
	[, array tracker_server, array storage_server])
delete file from storage server
parameters:
	file_id: the file id to be deleted
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


string fastdfs_storage_download_file_to_buff(string group_name, 
	string remote_filename [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
get file content from storage server
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return the file content for success, false for error


string fastdfs_storage_download_file_to_buff1(string file_id
        [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
get file content from storage server
parameters:
	file_id: the file id of the file
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return the file content for success, false for error


boolean fastdfs_storage_download_file_to_file(string group_name, 
	string remote_filename, string local_filename [, long file_offset, 
	long download_bytes, array tracker_server, array storage_server])
download file from storage server to local file 
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	local_filename: the local filename to save the file content
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean fastdfs_storage_download_file_to_file1(string file_id, 
	string local_filename [, long file_offset, long download_bytes, 
	array tracker_server, array storage_server])
download file from storage server to local file 
parameters:
	file_id: the file id of the file
	local_filename: the local filename to save the file content
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean fastdfs_storage_download_file_to_callback(string group_name,
	string remote_filename, array download_callback [, long file_offset, 
	long download_bytes, array tracker_server, array storage_server])
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	download_callback: the download callback array, elements as:
			callback  - the php callback function name
                                    callback function prototype as:
				    function my_download_file_callback($args, $file_size, $data)
			args      - use argument for callback function
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean fastdfs_storage_download_file_to_callback1(string file_id,
	array download_callback [, long file_offset, long download_bytes, 
	array tracker_server, array storage_server])
parameters:
	file_id: the file id of the file
	download_callback: the download callback array, elements as:
			callback  - the php callback function name
                                    callback function prototype as:
				    function my_download_file_callback($args, $file_size, $data)
			args      - use argument for callback function
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean fastdfs_storage_set_metadata(string group_name, string remote_filename,
	array meta_list [, string op_type, array tracker_server, 
	array storage_server])
set meta data of the file
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	meta_list: meta data assoc array to be set, such as
                   array('width'=>1024, 'height'=>768)
	op_type: operate flag, can be one of following flags:
		FDFS_STORAGE_SET_METADATA_FLAG_MERGE: combined with the old meta data
		FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE: overwrite the old meta data
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean fastdfs_storage_set_metadata1(string file_id, array meta_list
	[, string op_type, array tracker_server, array storage_server])
set meta data of the file
parameters:
	file_id: the file id of the file
	meta_list: meta data assoc array to be set, such as
                   array('width'=>1024, 'height'=>768)
	op_type: operate flag, can be one of following flags:
		FDFS_STORAGE_SET_METADATA_FLAG_MERGE: combined with the old meta data
		FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE: overwrite the old meta data
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


array fastdfs_storage_get_metadata(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
get meta data of the file
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       returned array like: array('width' => 1024, 'height' => 768)


array fastdfs_storage_get_metadata1(string file_id
	[, array tracker_server, array storage_server])
get meta data of the file
parameters:
	file_id: the file id of the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       returned array like: array('width' => 1024, 'height' => 768)


array fastdfs_connect_server(string ip_addr, int port)
connect to the server
parameters:
	ip_addr: the ip address of the server
	port: the port of the server
return assoc array for success, false for error


boolean fastdfs_disconnect_server(array server_info)
disconnect from the server
parameters:
	server_info: the assoc array including elements:
                     ip_addr, port and sock
return true for success, false for error


boolean fastdfs_active_test(array server_info)
send ACTIVE_TEST cmd to the server
parameters:
	server_info: the assoc array including elements:
                     ip_addr, port and sock, sock must be connected
return true for success, false for error


array fastdfs_tracker_get_connection()
get a connected tracker server
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock


boolean fastdfs_tracker_make_all_connections()
connect to all tracker servers
return true for success, false for error


boolean fastdfs_tracker_close_all_connections()
close all connections to the tracker servers
return true for success, false for error


array fastdfs_tracker_list_groups([string group_name, array tracker_server])
get group stat info
parameters:
	group_name: specify the group name, null or empty string means all groups
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return index array for success, false for error, each group as a array element


array fastdfs_tracker_query_storage_store([string group_name, 
		array tracker_server])
get the storage server info to upload file
parameters:
	group_name: specify the group name
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. the assoc array including
       elements: ip_addr, port, sock and store_path_index


array fastdfs_tracker_query_storage_store_list([string group_name, 
		array tracker_server])
get the storage server list to upload file
parameters:
	group_name: specify the group name
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return indexed storage server array for success, false for error.
       each element is an ssoc array including elements: 
       ip_addr, port, sock and store_path_index


array fastdfs_tracker_query_storage_update(string group_name, 
		string remote_filename [, array tracker_server])
get the storage server info to set metadata
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock


array fastdfs_tracker_query_storage_update1(string file_id, 
		[, array tracker_server])
get the storage server info to set metadata
parameters:
	file_id: the file id of the file
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock


array fastdfs_tracker_query_storage_fetch(string group_name, 
		string remote_filename [, array tracker_server])
get the storage server info to download file (or get metadata)
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock

array fastdfs_tracker_query_storage_fetch1(string file_id 
		[, array tracker_server])
get the storage server info to download file (or get metadata)
parameters:
	file_id: the file id of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock


array fastdfs_tracker_query_storage_list(string group_name, 
		string remote_filename [, array tracker_server])
get the storage server list which can retrieve the file content or metadata
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return index array for success, false for error.
       each server as an array element


array fastdfs_tracker_query_storage_list1(string file_id
		[, array tracker_server])
get the storage server list which can retrieve the file content or metadata
parameters:
	file_id: the file id of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return index array for success, false for error. 
       each server as an array element


boolean fastdfs_tracker_delete_storage(string group_name, string storage_ip)
delete the storage server from the cluster
parameters:
	group_name: the group name of the storage server
	storage_ip: the ip address of the storage server to be deleted
return true for success, false for error


FastDFS Class Info:

class FastDFS([int config_index, boolean bMultiThread]);
FastDFS class constructor
params:
        config_index: use which config file, base 0. default is 0
        bMultiThread: if in multi-thread, default is false


long FastDFS::get_last_error_no()
return last error no


string FastDFS::get_last_error_info()
return last error info

bool FastDFS::send_data(int sock, string buff)
parameters:
	sock: the unix socket description
	buff: the buff to send
return true for success, false for error


string FastDFS::http_gen_token(string remote_filename, int timestamp)
generate anti-steal token for HTTP download
parameters:
	remote_filename: the remote filename (do NOT including group name)
	timestamp: the timestamp (unix timestamp)
return token string for success, false for error


array FastDFS::get_file_info(string group_name, string filename)
get file info from the filename
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
return assoc array for success, false for error. 
	the assoc array including following elements:
		create_timestamp: the file create timestamp (unix timestamp)
		file_size: the file size (bytes)
		source_ip_addr: the source storage server ip address
		crc32: the crc32 signature of the file


array FastDFS::get_file_info1(string file_id)
get file info from the file id
parameters:
	file_id: the file id (including group name and filename) or remote filename
return assoc array for success, false for error. 
	the assoc array including following elements:
		create_timestamp: the file create timestamp (unix timestamp)
		file_size: the file size (bytes)
		source_ip_addr: the source storage server ip address


string FastDFS::gen_slave_filename(string master_filename, string prefix_name
                [, string file_ext_name])
generate slave filename by master filename, prefix name and file extension name
parameters:
	master_filename: the master filename / file id to generate 
			the slave filename
	prefix_name: the prefix name  to generate the slave filename
	file_ext_name: slave file extension name, can be null or emtpy 
			(do not including dot)
return slave filename string for success, false for error


boolean FastDFS::storage_file_exist(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
check file exist
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for exist, false for not exist


boolean FastDFS::storage_file_exist1(string file_id
	[, array tracker_server, array storage_server])
parameters:
	file_id: the file id of the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for exist, false for not exist


array FastDFS::storage_upload_by_filename(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload local file to storage server
parameters:
	local_filename: the local filename
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string FastDFS::storage_upload_by_filename1(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload local file to storage server
parameters:
	local_filename: the local filename
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error.


array FastDFS::storage_upload_by_filebuff(string file_buff
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file buff to storage server
parameters:
	file_buff: the file content
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string FastDFS::storage_upload_by_filebuff1(string file_buff
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file buff to storage server
parameters:
	file_buff: the file content
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


array FastDFS::storage_upload_by_callback(array callback_array
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file to storage server by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


array FastDFS::storage_upload_by_callback1(array callback_array
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file to storage server by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


array FastDFS::storage_upload_appender_by_filename(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload local file to storage server as appender file
parameters:
	local_filename: the local filename
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string FastDFS::storage_upload_appender_by_filename1(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload local file to storage server as appender file
parameters:
	local_filename: the local filename
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error.


array FastDFS::storage_upload_appender_by_filebuff(string file_buff
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file buff to storage server as appender file
parameters:
	file_buff: the file content
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string FastDFS::storage_upload_appender_by_filebuff1(string file_buff
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file buff to storage server as appender file
parameters:
	file_buff: the file content
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


array FastDFS::storage_upload_appender_by_callback(array callback_array
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file to storage server by callback as appender file
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string FastDFS::storage_upload_appender_by_callback1(array callback_array
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
upload file to storage server by callback as appender file
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	group_name: specify the group name to store the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


boolean FastDFS::storage_append_by_filename(string local_filename, 
	string group_name, appender_filename
	[, array tracker_server, array storage_server])
append local file to the appender file of storage server
parameters:
	local_filename: the local filename
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



string FastDFS::storage_upload_by_filename1(string local_filename, 
	[string appender_file_id, array tracker_server, array storage_server])
append local file to the appender file of storage server
parameters:
	local_filename: the local filename
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_append_by_filebuff(string file_buff, 
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
append file buff to the appender file of storage server
parameters:
	file_buff: the file content
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_append_by_filebuff1(string file_buff, 
	string appender_file_id [, array tracker_server, array storage_server])
append file buff to the appender file of storage server
parameters:
	file_buff: the file content
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_append_by_callback(array callback_array,
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
append file to the appender file of storage server by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_append_by_callback1(array callback_array,
	string appender_file_id [, array tracker_server, array storage_server])
append file buff to the appender file of storage server
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_modify_by_filename(string local_filename, 
	long file_offset, string group_name, appender_filename, 
	[array tracker_server, array storage_server])
modify appender file by local file
parameters:
	local_filename: the local filename
        file_offset: offset of appender file
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_modify_by_filename1(string local_filename, 
	long file_offset, string appender_file_id
        [, array tracker_server, array storage_server])
modify appender file by local file
parameters:
	local_filename: the local filename
        file_offset: offset of appender file
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_modify_by_filebuff(string file_buff, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
modify appender file by file buff
parameters:
	file_buff: the file content
        file_offset: offset of appender file
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_modify_by_filebuff1(string file_buff, 
	long file_offset, string appender_file_id
	[, array tracker_server, array storage_server])
modify appender file by file buff
parameters:
	file_buff: the file content
        file_offset: offset of appender file
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_modify_by_callback(array callback_array, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
modify appender file by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
        file_offset: offset of appender file
	group_name: the the group name of appender file
	appender_filename: the appender filename
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_modify_by_callback1(array callback_array, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
modify appender file by callback
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
        file_offset: offset of appender file
	appender_file_id: the appender file id
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_truncate_file(string group_name, 
	string appender_filename [, long truncated_file_size = 0, 
	array tracker_server, array storage_server])
truncate appender file to specify size
parameters:
	group_name: the the group name of appender file
	appender_filename: the appender filename
        truncated_file_size: truncate the file size to
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



boolean FastDFS::storage_truncate_file1(string appender_file_id
	[, long truncated_file_size = 0, array tracker_server, 
	array storage_server])
truncate appender file to specify size
parameters:
	appender_file_id: the appender file id
        truncated_file_size: truncate the file size to
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error



array FastDFS::storage_upload_slave_by_filename(string local_filename, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload local file to storage server (slave file mode)
parameters:
	file_buff: the file content
	group_name: the group name of the master file
	master_filename: the master filename to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string FastDFS::storage_upload_slave_by_filename1(string local_filename, 
	string master_file_id, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload local file to storage server (slave file mode)
parameters:
	local_filename: the local filename
	master_file_id: the master file id to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error.


array FastDFS::storage_upload_slave_by_filebuff(string file_buff, 
	string group_name, string master_filename, string prefix_name 
	[, file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload file buff to storage server (slave file mode)
parameters:
	file_buff: the file content
	group_name: the group name of the master file
	master_filename: the master filename to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string FastDFS::storage_upload_slave_by_filebuff1(string file_buff, 
	string master_file_id, string prefix_name
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload file buff to storage server (slave file mode)
parameters:
	file_buff: the file content
	master_file_id: the master file id to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


array FastDFS::storage_upload_slave_by_callback(array callback_array,
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload file to storage server by callback (slave file mode)
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	group_name: the group name of the master file
	master_filename: the master filename to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. 
       the returned array includes elements: group_name and filename


string FastDFS::storage_upload_slave_by_callback1(array callback_array,
	string master_file_id, string prefix_name
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
upload file to storage server by callback (slave file mode)
parameters:
	callback_array: the callback assoc array, must have keys:
			callback  - the php callback function name
                                    callback function prototype as:
				    function upload_file_callback($sock, $args)
			file_size - the file size
			args      - use argument for callback function
	master_file_id: the master file id to generate the slave file id
	prefix_name: the prefix name to generage the slave file id
	file_ext_name: the file extension name, do not include dot(.)
	meta_list: meta data assoc array, such as
                   array('width'=>1024, 'height'=>768)
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return file_id for success, false for error


boolean FastDFS::storage_delete_file(string group_name, string remote_filename 
	[, array tracker_server, array storage_server])
delete file from storage server
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean FastDFS::storage_delete_file1(string file_id
	[, array tracker_server, array storage_server])
delete file from storage server
parameters:
	file_id: the file id to be deleted
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


string FastDFS::storage_download_file_to_buff(string group_name, 
	string remote_filename [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
get file content from storage server
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return the file content for success, false for error


string FastDFS::storage_download_file_to_buff1(string file_id
        [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
get file content from storage server
parameters:
	file_id: the file id of the file
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return the file content for success, false for error


boolean FastDFS::storage_download_file_to_file(string group_name, 
	string remote_filename, string local_filename [, long file_offset, 
	long download_bytes, array tracker_server, array storage_server])
download file from storage server to local file 
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	local_filename: the local filename to save the file content
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean FastDFS::storage_download_file_to_file1(string file_id, 
	string local_filename [, long file_offset, long download_bytes, 
	array tracker_server, array storage_server])
download file from storage server to local file 
parameters:
	file_id: the file id of the file
	local_filename: the local filename to save the file content
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean FastDFS::storage_download_file_to_callback(string group_name,
	string remote_filename, array download_callback [, long file_offset, 
	long download_bytes, array tracker_server, array storage_server])
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	download_callback: the download callback array, elements as:
			callback  - the php callback function name
                                    callback function prototype as:
				    function my_download_file_callback($args, $file_size, $data)
			args      - use argument for callback function
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean FastDFS::storage_download_file_to_callback1(string file_id,
	array download_callback [, long file_offset, long download_bytes, 
	array tracker_server, array storage_server])
parameters:
	file_id: the file id of the file
	download_callback: the download callback array, elements as:
			callback  - the php callback function name
                                    callback function prototype as:
				    function my_download_file_callback($args, $file_size, $data)
			args      - use argument for callback function
	file_offset: file start offset, default value is 0
	download_bytes: 0 (default value) means from the file offset to 
                        the file end
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean FastDFS::storage_set_metadata(string group_name, string remote_filename,
	array meta_list [, string op_type, array tracker_server, 
	array storage_server])
set meta data of the file
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	meta_list: meta data assoc array to be set, such as
                   array('width'=>1024, 'height'=>768)
	op_type: operate flag, can be one of following flags:
		FDFS_STORAGE_SET_METADATA_FLAG_MERGE: combined with the old meta data
		FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE: overwrite the old meta data
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


boolean FastDFS::storage_set_metadata1(string file_id, array meta_list
	[, string op_type, array tracker_server, array storage_server])
set meta data of the file
parameters:
	file_id: the file id of the file
	meta_list: meta data assoc array to be set, such as
                   array('width'=>1024, 'height'=>768)
	op_type: operate flag, can be one of following flags:
		FDFS_STORAGE_SET_METADATA_FLAG_MERGE: combined with the old meta data
		FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE: overwrite the old meta data
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return true for success, false for error


array FastDFS::storage_get_metadata(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
get meta data of the file
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       returned array like: array('width' => 1024, 'height' => 768)


array FastDFS::storage_get_metadata1(string file_id
	[, array tracker_server, array storage_server])
get meta data of the file
parameters:
	file_id: the file id of the file
	tracker_server: the tracker server assoc array including elements: 
                        ip_addr, port and sock
	storage_server: the storage server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       returned array like: array('width' => 1024, 'height' => 768)


array FastDFS::connect_server(string ip_addr, int port)
connect to the server
parameters:
	ip_addr: the ip address of the server
	port: the port of the server
return assoc array for success, false for error


boolean FastDFS::disconnect_server(array server_info)
disconnect from the server
parameters:
	server_info: the assoc array including elements:
                     ip_addr, port and sock
return true for success, false for error


array FastDFS::tracker_get_connection()
get a connected tracker server
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock


boolean FastDFS::active_test(array server_info)
send ACTIVE_TEST cmd to the server
parameters:
	server_info: the assoc array including elements:
                     ip_addr, port and sock, sock must be connected
return true for success, false for error


boolean FastDFS::tracker_make_all_connections()
connect to all tracker servers
return true for success, false for error


boolean FastDFS::tracker_close_all_connections()
close all connections to the tracker servers
return true for success, false for error


array FastDFS::tracker_list_groups([string group_name, array tracker_server])
get group stat info
parameters:
	group_name: specify the group name, null or empty string means all groups
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return index array for success, false for error, each group as a array element


array FastDFS::tracker_query_storage_store([string group_name, 
		array tracker_server])
get the storage server info to upload file
parameters:
	group_name: specify the group name
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error. the assoc array including
       elements: ip_addr, port, sock and store_path_index


array FastDFS::tracker_query_storage_store_list([string group_name, 
		array tracker_server])
get the storage server list to upload file
parameters:
	group_name: specify the group name
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return indexed storage server array for success, false for error.
       each element is an ssoc array including elements: 
       ip_addr, port, sock and store_path_index


array FastDFS::tracker_query_storage_update(string group_name, 
		string remote_filename [, array tracker_server])
get the storage server info to set metadata
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock


array FastDFS::tracker_query_storage_update1(string file_id, 
		[, array tracker_server])
get the storage server info to set metadata
parameters:
	file_id: the file id of the file
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock


array FastDFS::tracker_query_storage_fetch(string group_name, 
		string remote_filename [, array tracker_server])
get the storage server info to download file (or get metadata)
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock

array FastDFS::tracker_query_storage_fetch1(string file_id 
		[, array tracker_server])
get the storage server info to download file (or get metadata)
parameters:
	file_id: the file id of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return assoc array for success, false for error
       the assoc array including elements: ip_addr, port and sock


array FastDFS::tracker_query_storage_list(string group_name, 
		string remote_filename [, array tracker_server])
get the storage server list which can retrieve the file content or metadata
parameters:
	group_name: the group name of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return index array for success, false for error.
       each server as an array element


array FastDFS::tracker_query_storage_list1(string file_id
		[, array tracker_server])
get the storage server list which can retrieve the file content or metadata
parameters:
	file_id: the file id of the file
	remote_filename: the filename on the storage server
	tracker_server: the tracker server assoc array including elements:
                        ip_addr, port and sock
return index array for success, false for error. 
       each server as an array element


boolean  FastDFS::tracker_delete_storage(string group_name, string storage_ip)
delete the storage server from the cluster
parameters:
	group_name: the group name of the storage server
	storage_ip: the ip address of the storage server to be deleted
return true for success, false for error

void FastDFS::close()
close tracker connections

