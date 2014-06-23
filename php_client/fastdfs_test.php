<?php
 $group_name = "group1";
 $remote_filename = "M00/28/E3/U6Q-CkrMFUgAAAAAAAAIEBucRWc5452.h";
 $file_id = $group_name . FDFS_FILE_ID_SEPERATOR . $remote_filename;

 echo fastdfs_client_version() . "\n";

 /*
 $file_id = $group_name . FDFS_FILE_ID_SEPERATOR . 'M00/00/02/wKjRbExc_qIAAAAAAABtNw6hsnM56585.part2.c';

 var_dump(fastdfs_get_file_info1($file_id));
 exit(1);
 */

 echo 'fastdfs_tracker_make_all_connections result: ' . fastdfs_tracker_make_all_connections() . "\n";
 var_dump(fastdfs_tracker_list_groups());

 $tracker = fastdfs_tracker_get_connection();
 var_dump($tracker);

 if (!fastdfs_active_test($tracker))
 {
	error_log("fastdfs_active_test errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info());
	exit(1);
 }

 $server = fastdfs_connect_server($tracker['ip_addr'], $tracker['port']); 
 var_dump($server);
 var_dump(fastdfs_disconnect_server($server));
 var_dump($server);

 var_dump(fastdfs_tracker_query_storage_store_list());

 $storage = fastdfs_tracker_query_storage_store();
 if (!$storage)
 {
	error_log("fastdfs_tracker_query_storage_store errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info());
	exit(1);
 }
 
 $server = fastdfs_connect_server($storage['ip_addr'], $storage['port']);
 if (!$server)
 {
        error_log("fastdfs_connect_server errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info());
        exit(1);
 }
 if (!fastdfs_active_test($server))
 {
	error_log("fastdfs_active_test errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info());
	exit(1);
 }

 //var_dump(fastdfs_tracker_list_groups($tracker));

 $storage['sock'] = $server['sock'];
 $file_info = fastdfs_storage_upload_by_filename("/usr/include/stdio.h", null, array(), null, $tracker, $storage);
 if ($file_info)
 {
	$group_name = $file_info['group_name'];
	$remote_filename = $file_info['filename'];

	var_dump($file_info);
	var_dump(fastdfs_get_file_info($group_name, $remote_filename));
	echo "file exist: " . fastdfs_storage_file_exist($group_name, $remote_filename) . "\n";

	$master_filename = $remote_filename;
	$prefix_name = '.part1';
	$slave_file_info = fastdfs_storage_upload_slave_by_filename("/usr/include/stdio.h", 
		$group_name, $master_filename, $prefix_name);
        if ($slave_file_info !== false)
        {
        var_dump($slave_file_info);

        $generated_filename = fastdfs_gen_slave_filename($master_filename, $prefix_name);
        if ($slave_file_info['filename'] != $generated_filename)
        {
                echo "${slave_file_info['filename']}\n != \n${generated_filename}\n";
        }

        echo "delete slave file return: " . fastdfs_storage_delete_file($slave_file_info['group_name'], $slave_file_info['filename']) . "\n";
        }
        else
        {
                echo "fastdfs_storage_upload_slave_by_filename fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
        }

	echo "delete file return: " . fastdfs_storage_delete_file($file_info['group_name'], $file_info['filename']) . "\n";
 }

 $file_id = fastdfs_storage_upload_by_filename1("/usr/include/stdio.h", null, array('width'=>1024, 'height'=>800, 'font'=>'Aris', 'Homepage' => true, 'price' => 103.75, 'status' => FDFS_STORAGE_STATUS_ACTIVE), '', $tracker, $storage);
 if ($file_id)
 {
	$master_file_id = $file_id;
	$prefix_name = '.part2';
	$slave_file_id = fastdfs_storage_upload_slave_by_filename1("/usr/include/stdio.h", 
		$master_file_id, $prefix_name);
	if ($slave_file_id !== false)
	{
	var_dump($slave_file_id);

	$generated_file_id = fastdfs_gen_slave_filename($master_file_id, $prefix_name);
	if ($slave_file_id != $generated_file_id)
	{
		echo "${slave_file_id}\n != \n${generated_file_id}\n";
	}

	echo "delete file $slave_file_id return: " . fastdfs_storage_delete_file1($slave_file_id) . "\n";
	}
        else
        {
                echo "fastdfs_storage_upload_slave_by_filename1 fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
        }

	echo "delete file $file_id return: " . fastdfs_storage_delete_file1($file_id) . "\n";
 }

 $file_info = fastdfs_storage_upload_by_filebuff("this is a test.", "txt");
 if ($file_info)
 {
	$group_name = $file_info['group_name'];
	$remote_filename = $file_info['filename'];

	var_dump($file_info);
	var_dump(fastdfs_get_file_info($group_name, $remote_filename));
	echo "file exist: " . fastdfs_storage_file_exist($group_name, $remote_filename) . "\n";

	$ts = time();
	$token = fastdfs_http_gen_token($remote_filename, $ts);
	echo "token=$token\n";

	$file_content = fastdfs_storage_download_file_to_buff($file_info['group_name'], $file_info['filename']);
	echo "file content: " . $file_content . "(" . strlen($file_content) . ")\n";
 	$local_filename = 't1.txt';
	echo 'storage_download_file_to_file result: ' . 
		fastdfs_storage_download_file_to_file($file_info['group_name'], $file_info['filename'], $local_filename) . "\n";

	echo "fastdfs_storage_set_metadata result: " . fastdfs_storage_set_metadata( 
		$file_info['group_name'], $file_info['filename'], 
		array('color'=>'', 'size'=>32, 'font'=>'MS Serif'), FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE) . "\n";

	$meta_list = fastdfs_storage_get_metadata($file_info['group_name'], $file_info['filename']);
	var_dump($meta_list);

	$master_filename = $remote_filename;
	$prefix_name = '.part1';
	$file_ext_name = 'txt';
	$slave_file_info = fastdfs_storage_upload_slave_by_filebuff('this is slave file.', 
		$group_name, $master_filename, $prefix_name, $file_ext_name);
        if ($slave_file_info !== false)
        {
        var_dump($slave_file_info);

        $generated_filename = fastdfs_gen_slave_filename($master_filename, $prefix_name, $file_ext_name);
        if ($slave_file_info['filename'] != $generated_filename)
        {
                echo "${slave_file_info['filename']}\n != \n${generated_filename}\n";
        }

        echo "delete slave file return: " . fastdfs_storage_delete_file($slave_file_info['group_name'], $slave_file_info['filename']) . "\n";
        }
        else
        {
                echo "fastdfs_storage_upload_slave_by_filebuff fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
        }

	echo "delete file return: " . fastdfs_storage_delete_file($file_info['group_name'], $file_info['filename']) . "\n";
 }

 $file_id = fastdfs_storage_upload_by_filebuff1("this\000is\000a\000test.", "bin", 
		array('width'=>1024, 'height'=>768, 'font'=>'Aris'));
 if ($file_id)
 {
	$file_content = fastdfs_storage_download_file_to_buff1($file_id);
	echo "file content: " . $file_content . "(" . strlen($file_content) . ")\n";
 	$local_filename = 't2.txt';
	echo 'storage_download_file_to_file1 result: ' . 
		fastdfs_storage_download_file_to_file1($file_id, $local_filename) . "\n";
	echo "fastdfs_storage_set_metadata1 result: " . fastdfs_storage_set_metadata1(
		$file_id, array('color'=>'yellow', 'size'=>'1234567890', 'font'=>'MS Serif'), 
		FDFS_STORAGE_SET_METADATA_FLAG_MERGE) . "\n";
	$meta_list = fastdfs_storage_get_metadata1($file_id);
	var_dump($meta_list);

	$master_file_id = $file_id;
	$prefix_name = '.part2';
	$file_ext_name = 'txt';
	$slave_file_id = fastdfs_storage_upload_slave_by_filebuff1('this is slave file1.', 
		$master_file_id, $prefix_name, $file_ext_name);
	if ($slave_file_id !== false)
	{
	var_dump($slave_file_id);

	$generated_file_id = fastdfs_gen_slave_filename($master_file_id, $prefix_name, $file_ext_name);
	if ($slave_file_id != $generated_file_id)
	{
		echo "${slave_file_id}\n != \n${generated_file_id}\n";
	}

	echo "delete file $slave_file_id return: " . fastdfs_storage_delete_file1($slave_file_id) . "\n";
	}
        else
        {
                echo "fastdfs_storage_upload_slave_by_filebuff1 fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
        }

	echo "delete file $file_id return: " . fastdfs_storage_delete_file1($file_id) . "\n";
 }

 var_dump(fastdfs_tracker_query_storage_update($group_name, $remote_filename));
 var_dump(fastdfs_tracker_query_storage_fetch($group_name, $remote_filename));
 var_dump(fastdfs_tracker_query_storage_list($group_name, $remote_filename));
 var_dump(fastdfs_tracker_query_storage_update1($file_id));
 var_dump(fastdfs_tracker_query_storage_fetch1($file_id, $tracker));
 var_dump(fastdfs_tracker_query_storage_list1($file_id, $tracker));

 echo "fastdfs_tracker_close_all_connections result: " . fastdfs_tracker_close_all_connections() . "\n";

 $fdfs = new FastDFS();
 echo 'tracker_make_all_connections result: ' . $fdfs->tracker_make_all_connections() . "\n";
 $tracker = $fdfs->tracker_get_connection();
 var_dump($tracker);

 $server = $fdfs->connect_server($tracker['ip_addr'], $tracker['port']);
 var_dump($server);
 var_dump($fdfs->disconnect_server($server));

 var_dump($fdfs->tracker_query_storage_store_list());

 //var_dump($fdfs->tracker_list_groups());
 //var_dump($fdfs->tracker_query_storage_store());
 //var_dump($fdfs->tracker_query_storage_update($group_name, $remote_filename));
 //var_dump($fdfs->tracker_query_storage_fetch($group_name, $remote_filename));
 //var_dump($fdfs->tracker_query_storage_list($group_name, $remote_filename));

 var_dump($fdfs->tracker_query_storage_update1($file_id));
 var_dump($fdfs->tracker_query_storage_fetch1($file_id));
 var_dump($fdfs->tracker_query_storage_list1($file_id));

 $file_info = $fdfs->storage_upload_by_filename("/usr/include/stdio.h");
 if ($file_info)
 {
	$group_name = $file_info['group_name'];
	$remote_filename = $file_info['filename'];

	var_dump($file_info);
	var_dump($fdfs->get_file_info($group_name, $remote_filename));
	echo "file exist: " . $fdfs->storage_file_exist($group_name, $remote_filename) . "\n";

	$master_filename = $remote_filename;
	$prefix_name = '.part1';
	$slave_file_info = $fdfs->storage_upload_slave_by_filename("/usr/include/stdio.h", 
		$group_name, $master_filename, $prefix_name);
        if ($slave_file_info !== false)
        {
        var_dump($slave_file_info);

        $generated_filename = $fdfs->gen_slave_filename($master_filename, $prefix_name);
        if ($slave_file_info['filename'] != $generated_filename)
        {
                echo "${slave_file_info['filename']}\n != \n${generated_filename}\n";
        }

        echo "delete slave file return: " . $fdfs->storage_delete_file($slave_file_info['group_name'], $slave_file_info['filename']) . "\n";
        }
        else
        {
                echo "storage_upload_slave_by_filename fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
        }

	echo "delete file return: " . $fdfs->storage_delete_file($file_info['group_name'], $file_info['filename']) . "\n";
 }

 $file_ext_name = 'c';
 $file_id = $fdfs->storage_upload_by_filename1("/usr/include/stdio.h", $file_ext_name, array('width'=>1024, 'height'=>800, 'font'=>'Aris'));
 if ($file_id)
 {
	$master_file_id = $file_id;
	$prefix_name = '.part2';
	$slave_file_id = $fdfs->storage_upload_slave_by_filename1("/usr/include/stdio.h", 
		$master_file_id, $prefix_name, $file_ext_name);
	if ($slave_file_id !== false)
	{
	var_dump($slave_file_id);

	$generated_file_id = $fdfs->gen_slave_filename($master_file_id, $prefix_name, $file_ext_name);
	if ($slave_file_id != $generated_file_id)
	{
		echo "${slave_file_id}\n != \n${generated_file_id}\n";
	}

	echo "delete file $slave_file_id return: " . $fdfs->storage_delete_file1($slave_file_id) . "\n";
	}
        else
        {
                echo "fastdfs_storage_upload_slave_by_filename1 fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
        }

	echo "delete file $file_id return: " . $fdfs->storage_delete_file1($file_id) . "\n";
 }

 $file_info = $fdfs->storage_upload_by_filebuff("", "txt");
 if ($file_info)
 {
	var_dump($file_info);
	$file_content = $fdfs->storage_download_file_to_buff($file_info['group_name'], $file_info['filename']);
	echo "file content: " . $file_content . "(" . strlen($file_content) . ")\n";
 	$local_filename = 't3.txt';
	echo 'storage_download_file_to_file result: ' . 
		$fdfs->storage_download_file_to_file($file_info['group_name'], $file_info['filename'], $local_filename) . "\n";

	echo "storage_set_metadata result: " . $fdfs->storage_set_metadata( 
		$file_info['group_name'], $file_info['filename'], 
		array('color'=>'yellow', 'size'=>32), FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE) . "\n";

	$meta_list = $fdfs->storage_get_metadata($file_info['group_name'], $file_info['filename']);
	var_dump($meta_list);

	$master_filename = $file_info['filename'];
	$prefix_name = '.part1';
	$file_ext_name = 'txt';
	$slave_file_info = $fdfs->storage_upload_slave_by_filebuff('this is slave file  1 by class.', 
		$file_info['group_name'], $master_filename, $prefix_name, $file_ext_name);
        if ($slave_file_info !== false)
        {
        var_dump($slave_file_info);

        $generated_filename = $fdfs->gen_slave_filename($master_filename, $prefix_name, $file_ext_name);
        if ($slave_file_info['filename'] != $generated_filename)
        {
                echo "${slave_file_info['filename']}\n != \n${generated_filename}\n";
        }

        echo "delete slave file return: " . $fdfs->storage_delete_file($slave_file_info['group_name'], $slave_file_info['filename']) . "\n";
        }
        else
        {
                echo "storage_upload_slave_by_filebuff fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
        }

	echo "delete file return: " . $fdfs->storage_delete_file($file_info['group_name'], $file_info['filename']) . "\n";
 }

 $file_id = $fdfs->storage_upload_by_filebuff1("this\000is\001a\002test.", "bin", 
		array('color'=>'none', 'size'=>0, 'font'=>'Aris'));
 if ($file_id)
 {
	var_dump($fdfs->get_file_info1($file_id));
	echo "file exist: " . $fdfs->storage_file_exist1($file_id) . "\n";

	$ts = time();
	$token = $fdfs->http_gen_token($file_id, $ts);
	echo "token=$token\n";

	$file_content = $fdfs->storage_download_file_to_buff1($file_id);
	echo "file content: " . $file_content . "(" . strlen($file_content) . ")\n";
 	$local_filename = 't4.txt';
	echo 'storage_download_file_to_file1 result: ' . $fdfs->storage_download_file_to_file1($file_id, $local_filename) . "\n";
	echo "storage_set_metadata1 result: " . $fdfs->storage_set_metadata1( 
		$file_id, array('color'=>'yellow', 'size'=>32), FDFS_STORAGE_SET_METADATA_FLAG_MERGE) . "\n";

	$master_file_id = $file_id;
	$prefix_name = '.part2';
	$file_ext_name = 'txt';
	$slave_file_id = $fdfs->storage_upload_slave_by_filebuff1('this is slave file 2 by class.', 
		$master_file_id, $prefix_name, $file_ext_name);
	if ($slave_file_id !== false)
	{
	var_dump($slave_file_id);

	$generated_file_id = $fdfs->gen_slave_filename($master_file_id, $prefix_name, $file_ext_name);
	if ($slave_file_id != $generated_file_id)
	{
		echo "${slave_file_id}\n != \n${generated_file_id}\n";
	}

	echo "delete file $slave_file_id return: " . $fdfs->storage_delete_file1($slave_file_id) . "\n";
	}
        else
        {
                echo "storage_upload_slave_by_filebuff1 fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
        }

	$meta_list = $fdfs->storage_get_metadata1($file_id);
	if ($meta_list !== false)
	{
		var_dump($meta_list);
	}
	else
	{
		echo "errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
	}

        echo "delete file $file_id return: " . $fdfs->storage_delete_file1($file_id) . "\n";
 }

 var_dump($fdfs->active_test($tracker));
 echo 'tracker_close_all_connections result: ' . $fdfs->tracker_close_all_connections() . "\n";
?>
