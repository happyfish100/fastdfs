<?php
 define('FILE_BUFF', "this is a test\n");

 echo 'FastDFS Client Version: ' . fastdfs_client_version() . "\n";

 $upload_callback_arg = array ( 'buff' => FILE_BUFF);
 $upload_callback_array = array(
		'callback' => 'my_upload_file_callback', 
		'file_size' => strlen(FILE_BUFF), 
		'args' => $upload_callback_arg);

 $download_callback_arg = array (
		'filename' => '/tmp/out.txt',
		'write_bytes' => 0, 
		'fhandle' => NULL
		);
 $download_callback_array = array(
		'callback' => 'my_download_file_callback', 
		'args' => &$download_callback_arg);

 $file_info = fastdfs_storage_upload_by_callback($upload_callback_array);
 if ($file_info)
 {
	$group_name = $file_info['group_name'];
	$remote_filename = $file_info['filename'];

	var_dump($file_info);
	var_dump(fastdfs_get_file_info($group_name, $remote_filename));

	fastdfs_storage_download_file_to_callback($group_name, $remote_filename, $download_callback_array);
 }
 else
 {
	echo "upload file fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
 }

 $file_id = fastdfs_storage_upload_by_callback1($upload_callback_array, 'txt');
 if ($file_id)
 {
	var_dump($file_id);
	$download_callback_arg['filename'] = '/tmp/out1.txt';
	fastdfs_storage_download_file_to_callback1($file_id, $download_callback_array);
 }
 else
 {
	echo "upload file fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
 }

 $fdfs = new FastDFS();
 $file_info = $fdfs->storage_upload_by_callback($upload_callback_array, 'txt');
 if ($file_info)
 {
	$group_name = $file_info['group_name'];
	$remote_filename = $file_info['filename'];

	var_dump($file_info);
	var_dump($fdfs->get_file_info($group_name, $remote_filename));
	$download_callback_arg['filename'] = '/tmp/fdfs_out.txt';
	$fdfs->storage_download_file_to_callback($group_name, $remote_filename, $download_callback_array);
 }
 else
 {
	echo "upload file fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
 }

 $file_id = $fdfs->storage_upload_by_callback1($upload_callback_array, 'txt');
 if ($file_id)
 {
	var_dump($file_id);
	$download_callback_arg['filename'] = '/tmp/fdfs_out1.txt';
	$fdfs->storage_download_file_to_callback1($file_id, $download_callback_array);
 }
 else
 {
	echo "upload file fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
 }

 function my_upload_file_callback($sock, $args)
 {
	//var_dump($args);

	$ret = fastdfs_send_data($sock, $args['buff']);
	return $ret;
 }

 function my_download_file_callback($args, $file_size, $data)
 {
	//var_dump($args);

	if ($args['fhandle'] == NULL)
	{
		$args['fhandle'] = fopen ($args['filename'], 'w');
		if (!$args['fhandle'])
		{
			echo 'open file: ' . $args['filename'] . " fail!\n";
			return false;
		}
	}

	$len = strlen($data);
	if (fwrite($args['fhandle'], $data, $len) === false)
	{
		echo 'write to file: ' . $args['filename'] . " fail!\n";
		$result = false;
	}
	else
	{
		$args['write_bytes'] += $len;
		$result = true;
	}

	if ((!$result) || $args['write_bytes'] >= $file_size)
	{
		fclose($args['fhandle']);
		$args['fhandle'] = NULL;
		$args['write_bytes'] = 0;
	}
	
	return $result;
 }
?>
