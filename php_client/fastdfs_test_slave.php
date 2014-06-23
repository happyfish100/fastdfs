<?php
 $group_name = "group1";
 $remote_filename = "M00/28/E3/U6Q-CkrMFUgAAAAAAAAIEBucRWc5452.h";
 $file_id = $group_name . FDFS_FILE_ID_SEPERATOR . $remote_filename;

 echo fastdfs_client_version() . "\n";

 $storage = fastdfs_tracker_query_storage_store();
 if (!$storage)
 {
	error_log("errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info());
	exit(1);
 }
 
 $server = fastdfs_connect_server($storage['ip_addr'], $storage['port']);
 if (!$server)
 {
        error_log("errno1: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info());
        exit(1);
 }
 if (!fastdfs_active_test($server))
 {
	error_log("errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info());
	exit(1);
 }

 $storage['sock'] = $server['sock'];
 $file_info = fastdfs_storage_upload_by_filename("/usr/include/stdio.h", null, array('test' => 1));
 if ($file_info)
 {
	$group_name = $file_info['group_name'];
	$remote_filename = $file_info['filename'];

	var_dump($file_info);
	var_dump(fastdfs_get_file_info($group_name, $remote_filename));

	$master_filename = $remote_filename;
	$prefix_name = '.part1';
	$meta_list = array('width' => 1024, 'height' => 768, 'color' => 'blue');
	$slave_file_info = fastdfs_storage_upload_slave_by_filename("/usr/include/stdio.h", 
		$group_name, $master_filename, $prefix_name, null, $meta_list);
        if ($slave_file_info !== false)
        {
        var_dump($slave_file_info);

        $generated_filename = fastdfs_gen_slave_filename($master_filename, $prefix_name);
        if ($slave_file_info['filename'] != $generated_filename)
        {
                echo "${slave_file_info['filename']}\n != \n${generated_filename}\n";
        }

        //echo "delete slave file return: " . fastdfs_storage_delete_file($slave_file_info['group_name'], $slave_file_info['filename']) . "\n";
        }
        else
        {
                echo "fastdfs_storage_upload_slave_by_filename fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
        }

	echo "delete file return: " . fastdfs_storage_delete_file($file_info['group_name'], $file_info['filename']) . "\n";
 }

?>
