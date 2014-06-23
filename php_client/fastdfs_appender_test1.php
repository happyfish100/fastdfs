<?php
 echo fastdfs_client_version() . "\n";


 $appender_file_id = fastdfs_storage_upload_appender_by_filename1("/usr/include/stdio.h");
 if (!$appender_file_id)
 {
	echo "fastdfs_storage_upload_appender_by_filename1 fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
	exit;
 }

 var_dump($appender_file_id);
 var_dump(fastdfs_get_file_info1($appender_file_id));

 if (!fastdfs_storage_append_by_filename1("/usr/include/stdlib.h", $appender_file_id))
 {
	echo "fastdfs_storage_append_by_filename1 fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
	exit;
 }

 var_dump(fastdfs_get_file_info1($appender_file_id));

 if (!fastdfs_storage_modify_by_filename1("/usr/include/stdlib.h", 0, $appender_file_id))
 {
	echo "fastdfs_storage_modify_by_filename1 fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
	exit;
 }

 var_dump(fastdfs_get_file_info1($appender_file_id));

 if (!fastdfs_storage_truncate_file1($appender_file_id, 0))
 {
	echo "fastdfs_storage_truncate_file1 fail, errno: " . fastdfs_get_last_error_no() . ", error info: " . fastdfs_get_last_error_info() . "\n";
	exit;
 }

 var_dump(fastdfs_get_file_info1($appender_file_id));
 echo "function test done\n\n";

 $fdfs = new FastDFS();
 $appender_file_id = $fdfs->storage_upload_appender_by_filename1("/usr/include/stdio.h");
 if (!$appender_file_id)
 {
	echo "\$fdfs->storage_upload_appender_by_filename1 fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
	exit;
 }

 var_dump($appender_file_id);
 var_dump($fdfs->get_file_info1($appender_file_id));

 if (!$fdfs->storage_append_by_filename1("/usr/include/stdlib.h", $appender_file_id))
 {
	echo "\$fdfs->storage_append_by_filename1 fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
	exit;
 }

 var_dump($fdfs->get_file_info1($appender_file_id));

 if (!$fdfs->storage_modify_by_filename1("/usr/include/stdlib.h", 0, $appender_file_id))
 {
	echo "\$fdfs->storage_modify_by_filename1 fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
	exit;
 }

 var_dump($fdfs->get_file_info1($appender_file_id));

 if (!$fdfs->storage_truncate_file1($appender_file_id))
 {
	echo "\$fdfs->torage_truncate_file1 torage_modify_by_filename1 fail, errno: " . $fdfs->get_last_error_no() . ", error info: " . $fdfs->get_last_error_info() . "\n";
	exit;
 }

 var_dump($fdfs->get_file_info1($appender_file_id));

 echo 'tracker_close_all_connections result: ' . $fdfs->tracker_close_all_connections() . "\n";
?>
