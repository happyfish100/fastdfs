#include "php7_ext_wrapper.h"
#include "ext/standard/info.h"
#include <zend_extensions.h>
#include <zend_exceptions.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "fdfs_client.h"
#include "logger.h"
#include "sockopt.h"
#include "fdfs_global.h"
#include "shared_func.h"
#include "client_global.h"
#include "fastdfs_client.h"
#include "fdfs_http_shared.h"

typedef struct
{
	TrackerServerGroup *pTrackerGroup;
} FDFSConfigInfo;

typedef struct
{
	TrackerServerGroup *pTrackerGroup;
	int err_no;
} FDFSPhpContext;

typedef struct
{
#if PHP_MAJOR_VERSION < 7
	zend_object zo;
#endif
	FDFSConfigInfo *pConfigInfo;
	FDFSPhpContext context;
#if PHP_MAJOR_VERSION >= 7
	zend_object zo;
#endif
} php_fdfs_t;

typedef struct
{
	zval *func_name;
	zval *args;
} php_fdfs_callback_t;

typedef struct
{
	php_fdfs_callback_t callback;
	int64_t file_size;
} php_fdfs_upload_callback_t;


#if PHP_MAJOR_VERSION < 7
#define fdfs_get_object(obj) zend_object_store_get_object(obj)
#else
#define fdfs_get_object(obj) (void *)((char *)(Z_OBJ_P(obj)) - XtOffsetOf(php_fdfs_t, zo))
#endif


static int php_fdfs_download_callback(void *arg, const int64_t file_size, \
		const char *data, const int current_size);

static FDFSConfigInfo *config_list = NULL;
static int config_count = 0;

static FDFSPhpContext php_context = {&g_tracker_group, 0};

static zend_class_entry *fdfs_ce = NULL;
static zend_class_entry *fdfs_exception_ce = NULL;

#if PHP_MAJOR_VERSION >= 7
static zend_object_handlers fdfs_object_handlers;
#endif

#if HAVE_SPL
static zend_class_entry *spl_ce_RuntimeException = NULL;
#endif

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3)
const zend_fcall_info empty_fcall_info = { 0, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0 };
#undef ZEND_BEGIN_ARG_INFO_EX
#define ZEND_BEGIN_ARG_INFO_EX(name, pass_rest_by_reference, return_reference, required_num_args) \
    static zend_arg_info name[] = {                                                               \
        { NULL, 0, NULL, 0, 0, 0, pass_rest_by_reference, return_reference, required_num_args },
#endif


#define CLEAR_HASH_SOCK_FIELD(php_hash) \
	{ \
	    zval *sock_zval; \
	    MAKE_STD_ZVAL(sock_zval); \
	    ZVAL_LONG(sock_zval, -1); \
	\
	    zend_hash_update_wrapper(php_hash, "sock", sizeof("sock"), \
			    &sock_zval, sizeof(zval *), NULL); \
	}

// Every user visible function must have an entry in fastdfs_client_functions[].
	zend_function_entry fastdfs_client_functions[] = {
		ZEND_FE(fastdfs_client_version, NULL)
		ZEND_FE(fastdfs_active_test, NULL)
		ZEND_FE(fastdfs_connect_server, NULL)
		ZEND_FE(fastdfs_disconnect_server, NULL)
		ZEND_FE(fastdfs_get_last_error_no, NULL)
		ZEND_FE(fastdfs_get_last_error_info, NULL)
		ZEND_FE(fastdfs_tracker_get_connection, NULL)
		ZEND_FE(fastdfs_tracker_make_all_connections, NULL)
		ZEND_FE(fastdfs_tracker_close_all_connections, NULL)
		ZEND_FE(fastdfs_tracker_list_groups, NULL)
		ZEND_FE(fastdfs_tracker_query_storage_store, NULL)
		ZEND_FE(fastdfs_tracker_query_storage_store_list, NULL)
		ZEND_FE(fastdfs_tracker_query_storage_update, NULL)
		ZEND_FE(fastdfs_tracker_query_storage_fetch, NULL)
		ZEND_FE(fastdfs_tracker_query_storage_list, NULL)
		ZEND_FE(fastdfs_tracker_query_storage_update1, NULL)
		ZEND_FE(fastdfs_tracker_query_storage_fetch1, NULL)
		ZEND_FE(fastdfs_tracker_query_storage_list1, NULL)
		ZEND_FE(fastdfs_tracker_delete_storage, NULL)
		ZEND_FE(fastdfs_storage_upload_by_filename, NULL)
		ZEND_FE(fastdfs_storage_upload_by_filename1, NULL)
		ZEND_FE(fastdfs_storage_upload_by_filebuff, NULL)
		ZEND_FE(fastdfs_storage_upload_by_filebuff1, NULL)
		ZEND_FE(fastdfs_storage_upload_by_callback, NULL)
		ZEND_FE(fastdfs_storage_upload_by_callback1, NULL)
		ZEND_FE(fastdfs_storage_append_by_filename, NULL)
		ZEND_FE(fastdfs_storage_append_by_filename1, NULL)
		ZEND_FE(fastdfs_storage_append_by_filebuff, NULL)
		ZEND_FE(fastdfs_storage_append_by_filebuff1, NULL)
		ZEND_FE(fastdfs_storage_append_by_callback, NULL)
		ZEND_FE(fastdfs_storage_append_by_callback1, NULL)
		ZEND_FE(fastdfs_storage_modify_by_filename, NULL)
		ZEND_FE(fastdfs_storage_modify_by_filename1, NULL)
		ZEND_FE(fastdfs_storage_modify_by_filebuff, NULL)
		ZEND_FE(fastdfs_storage_modify_by_filebuff1, NULL)
		ZEND_FE(fastdfs_storage_modify_by_callback, NULL)
		ZEND_FE(fastdfs_storage_modify_by_callback1, NULL)
		ZEND_FE(fastdfs_storage_upload_appender_by_filename, NULL)
		ZEND_FE(fastdfs_storage_upload_appender_by_filename1, NULL)
		ZEND_FE(fastdfs_storage_upload_appender_by_filebuff, NULL)
		ZEND_FE(fastdfs_storage_upload_appender_by_filebuff1, NULL)
		ZEND_FE(fastdfs_storage_upload_appender_by_callback, NULL)
		ZEND_FE(fastdfs_storage_upload_appender_by_callback1, NULL)
		ZEND_FE(fastdfs_storage_upload_slave_by_filename, NULL)
		ZEND_FE(fastdfs_storage_upload_slave_by_filename1, NULL)
		ZEND_FE(fastdfs_storage_upload_slave_by_filebuff, NULL)
		ZEND_FE(fastdfs_storage_upload_slave_by_filebuff1, NULL)
		ZEND_FE(fastdfs_storage_upload_slave_by_callback, NULL)
		ZEND_FE(fastdfs_storage_upload_slave_by_callback1, NULL)
		ZEND_FE(fastdfs_storage_delete_file, NULL)
		ZEND_FE(fastdfs_storage_delete_file1, NULL)
		ZEND_FE(fastdfs_storage_truncate_file, NULL)
		ZEND_FE(fastdfs_storage_truncate_file1, NULL)
		ZEND_FE(fastdfs_storage_download_file_to_buff, NULL)
		ZEND_FE(fastdfs_storage_download_file_to_buff1, NULL)
		ZEND_FE(fastdfs_storage_download_file_to_file, NULL)
		ZEND_FE(fastdfs_storage_download_file_to_file1, NULL)
		ZEND_FE(fastdfs_storage_download_file_to_callback, NULL)
		ZEND_FE(fastdfs_storage_download_file_to_callback1, NULL)
		ZEND_FE(fastdfs_storage_set_metadata, NULL)
		ZEND_FE(fastdfs_storage_set_metadata1, NULL)
		ZEND_FE(fastdfs_storage_get_metadata, NULL)
		ZEND_FE(fastdfs_storage_get_metadata1, NULL)
		ZEND_FE(fastdfs_http_gen_token, NULL)
		ZEND_FE(fastdfs_get_file_info, NULL)
		ZEND_FE(fastdfs_get_file_info1, NULL)
		ZEND_FE(fastdfs_storage_file_exist, NULL)
		ZEND_FE(fastdfs_storage_file_exist1, NULL)
		ZEND_FE(fastdfs_gen_slave_filename, NULL)
		ZEND_FE(fastdfs_send_data, NULL)
		{NULL, NULL, NULL}  /* Must be the last line */
	};


zend_module_entry fastdfs_client_module_entry = {
	STANDARD_MODULE_HEADER,
	"fastdfs_client",
	fastdfs_client_functions,
	PHP_MINIT(fastdfs_client),
	PHP_MSHUTDOWN(fastdfs_client),
	NULL,//PHP_RINIT(fastdfs_client),
	NULL,//PHP_RSHUTDOWN(fastdfs_client),
	PHP_MINFO(fastdfs_client),
	"1.00", 
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_FASTDFS_CLIENT
	ZEND_GET_MODULE(fastdfs_client)
#endif

static int fastdfs_convert_metadata_to_array(zval *metadata_obj, \
		FDFSMetaData **meta_list, int *meta_count)
{
	HashTable *meta_hash;
	char *szKey;
	char *szValue;
	unsigned long index;
	unsigned int key_len;
	int value_len;
	HashPosition pointer;
	zval **data;
#if PHP_MAJOR_VERSION < 7
	zval ***ppp;
#else
	zval *for_php7;
#endif
	FDFSMetaData *pMetaData;

	meta_hash = Z_ARRVAL_P(metadata_obj);
	*meta_count = zend_hash_num_elements(meta_hash);
	if (*meta_count == 0)
	{
		*meta_list = NULL;
		return 0;
	}

	*meta_list = (FDFSMetaData *)malloc(sizeof(FDFSMetaData)*(*meta_count));
	if (*meta_list == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", __LINE__, \
			(int)sizeof(FDFSMetaData) * (*meta_count), \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}

	memset(*meta_list, 0, sizeof(FDFSMetaData) * (*meta_count));
	pMetaData = *meta_list;

#if PHP_MAJOR_VERSION < 7
	ppp = &data;
	for (zend_hash_internal_pointer_reset_ex(meta_hash, &pointer); \
		zend_hash_get_current_data_ex(meta_hash, (void **)ppp, &pointer)
		 == SUCCESS; zend_hash_move_forward_ex(meta_hash, &pointer))
#else
	data = &for_php7;
	for (zend_hash_internal_pointer_reset_ex(meta_hash, &pointer); \
		(for_php7=zend_hash_get_current_data_ex(meta_hash, &pointer))
		 != NULL; zend_hash_move_forward_ex(meta_hash, &pointer))
#endif
	{
#if PHP_MAJOR_VERSION < 7
		if (zend_hash_get_current_key_ex(meta_hash, &szKey, \
			 &(key_len), &index, 0, &pointer) != HASH_KEY_IS_STRING)
#else
		zend_string *_key_ptr;
		if (zend_hash_get_current_key_ex(meta_hash, &_key_ptr, \
			 (zend_ulong *)&index, &pointer) != HASH_KEY_IS_STRING)
#endif
		{
			logError("file: "__FILE__", line: %d, " \
				"invalid array element, " \
				"index=%ld!", __LINE__, index);

			free(*meta_list);
			*meta_list = NULL;
			*meta_count = 0;
			return EINVAL;
		}

#if PHP_MAJOR_VERSION >= 7
		szKey = _key_ptr->val;
		key_len = _key_ptr->len;
#endif
		if (key_len > FDFS_MAX_META_NAME_LEN)
		{
			key_len = FDFS_MAX_META_NAME_LEN;
		}
		memcpy(pMetaData->name, szKey, key_len);

		if (ZEND_TYPE_OF(*data) == IS_STRING)
		{
			szValue = Z_STRVAL_PP(data);
			value_len = Z_STRLEN_PP(data);

			if (value_len > FDFS_MAX_META_VALUE_LEN)
			{
				value_len = FDFS_MAX_META_VALUE_LEN;
			}
			memcpy(pMetaData->value, szValue, value_len);
		}
		else if (ZEND_TYPE_OF(*data) == IS_LONG || ZEND_IS_BOOL(*data))
		{
			sprintf(pMetaData->value, "%ld", (*data)->value.lval);
		}
		else if (ZEND_TYPE_OF(*data) == IS_DOUBLE)
		{
			sprintf(pMetaData->value, "%.2f", (*data)->value.dval);
		}
		else
		{
			logError("file: "__FILE__", line: %d, " \
				"invalid array element, key=%s, value type=%d",\
				 __LINE__, szKey, ZEND_TYPE_OF(*data));

			free(*meta_list);
			*meta_list = NULL;
			*meta_count = 0;
			return EINVAL;
		}

		pMetaData++;
	}

	return 0;
}

static void php_fdfs_tracker_get_connection_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int argc;
	ConnectionInfo *pTrackerServer;

	argc = ZEND_NUM_ARGS();
	if (argc != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker_get_connection parameters count: %d != 0", 
			__LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	pTrackerServer = tracker_get_connection_no_pool(pContext->pTrackerGroup);
	if (pTrackerServer == NULL)
	{
		pContext->err_no = ENOENT;
		RETURN_BOOL(false);
	}

	pContext->err_no = 0;
	array_init(return_value);
	
	zend_add_assoc_stringl_ex(return_value, "ip_addr", sizeof("ip_addr"), \
		pTrackerServer->ip_addr, strlen(pTrackerServer->ip_addr), 1);
	zend_add_assoc_long_ex(return_value, "port", sizeof("port"), \
		pTrackerServer->port);
	zend_add_assoc_long_ex(return_value, "sock", sizeof("sock"), \
		pTrackerServer->sock);
}

static void php_fdfs_tracker_make_all_connections_impl( \
		INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext)
{
	int argc;

	argc = ZEND_NUM_ARGS();
	if (argc != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker_make_all_connections parameters " \
			"count: %d != 0", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	pContext->err_no = tracker_get_all_connections_ex( \
				pContext->pTrackerGroup);
	if (pContext->err_no == 0)
	{
		RETURN_BOOL(true);
	}
	else
	{
		RETURN_BOOL(false);
	}
}

static void php_fdfs_tracker_close_all_connections_impl( \
		INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext)
{
	int argc;

	argc = ZEND_NUM_ARGS();
	if (argc != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker_close_all_connections parameters " \
			"count: %d != 0", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_close_all_connections_ex(pContext->pTrackerGroup);
	pContext->err_no = 0;
	RETURN_BOOL(true);
}

static void php_fdfs_connect_server_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int argc;
	char *ip_addr;
	zend_size_t ip_len;
	long port;
	ConnectionInfo server_info;

	argc = ZEND_NUM_ARGS();
	if (argc != 2)
	{
		logError("file: "__FILE__", line: %d, " \
			"fastdfs_connect_server parameters count: %d != 2", \
			__LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", \
				&ip_addr, &ip_len, &port) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	snprintf(server_info.ip_addr, sizeof(server_info.ip_addr), \
		"%s", ip_addr);
	server_info.port = port;
	server_info.sock = -1;

	if ((pContext->err_no=conn_pool_connect_server(&server_info, \
			g_fdfs_network_timeout)) == 0)
	{
		array_init(return_value);
		zend_add_assoc_stringl_ex(return_value, "ip_addr", \
			sizeof("ip_addr"), ip_addr, ip_len, 1);
		zend_add_assoc_long_ex(return_value, "port", sizeof("port"), \
			port);
		zend_add_assoc_long_ex(return_value, "sock", sizeof("sock"), \
			server_info.sock);
	}
	else
	{
		RETURN_BOOL(false);
	}
}

static void php_fdfs_disconnect_server_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int argc;
	zval *server_info;
	HashTable *tracker_hash;
	zval *data;
	int sock;

	argc = ZEND_NUM_ARGS();
	if (argc != 1)
	{
		logError("file: "__FILE__", line: %d, " \
			"fastdfs_disconnect_server parameters count: %d != 1", \
			__LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", \
				&server_info) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_hash = Z_ARRVAL_P(server_info);
	if (zend_hash_find_wrapper(tracker_hash, "sock", sizeof("sock"), \
			&data) == FAILURE)
	{
		pContext->err_no = ENOENT;
		RETURN_BOOL(false);
	}

	if (ZEND_TYPE_OF(data) == IS_LONG)
	{
		sock = data->value.lval;
		if (sock >= 0)
		{
			close(sock);
		}

		CLEAR_HASH_SOCK_FIELD(tracker_hash)

		pContext->err_no = 0;
		RETURN_BOOL(true);
	}
	else
	{
		logError("file: "__FILE__", line: %d, " \
			"sock type is invalid, type=%d!", \
			__LINE__, ZEND_TYPE_OF(data));
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}
}

static int php_fdfs_get_callback_from_hash(HashTable *callback_hash, \
		php_fdfs_callback_t *pCallback)
{
	zval *data;

	if (zend_hash_find_wrapper(callback_hash, "callback", sizeof("callback"), \
			&data) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"callback\" not exist!", __LINE__);
		return ENOENT;
	}
	if (ZEND_TYPE_OF(data) != IS_STRING)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"callback\" is not string type, type=%d!", \
			__LINE__, ZEND_TYPE_OF(data));
		return EINVAL;
	}
	pCallback->func_name = data;

	if (zend_hash_find_wrapper(callback_hash, "args", sizeof("args"), \
			&data) == FAILURE)
	{
		pCallback->args = NULL;
	}
	else
	{
		pCallback->args = (ZEND_TYPE_OF(data) == IS_NULL) ? NULL : data;
	}

	return 0;
}

static int php_fdfs_get_upload_callback_from_hash(HashTable *callback_hash, \
		php_fdfs_upload_callback_t *pUploadCallback)
{
	zval *data;
	int result;

	if ((result=php_fdfs_get_callback_from_hash(callback_hash, \
			&(pUploadCallback->callback))) != 0)
	{
		return result;
	}

	if (zend_hash_find_wrapper(callback_hash, "file_size", sizeof("file_size"), \
			&data) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"file_size\" not exist!", __LINE__);
		return ENOENT;
	}
	if (ZEND_TYPE_OF(data) != IS_LONG)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"file_size\" is not long type, type=%d!", \
			__LINE__, ZEND_TYPE_OF(data));
		return EINVAL;
	}
	pUploadCallback->file_size = data->value.lval;
	if (pUploadCallback->file_size < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"file_size: %"PRId64" is invalid!", \
			__LINE__, pUploadCallback->file_size);
		return EINVAL;
	}

	return 0;
}

static int php_fdfs_get_server_from_hash(HashTable *tracker_hash, \
		ConnectionInfo *pTrackerServer)
{
	zval *data;
	char *ip_addr;
	int ip_len;

	memset(pTrackerServer, 0, sizeof(ConnectionInfo));
	data = NULL;
	if (zend_hash_find_wrapper(tracker_hash, "ip_addr", sizeof("ip_addr"), \
			&data) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"ip_addr\" not exist!", __LINE__);
		return ENOENT;
	}
	if (ZEND_TYPE_OF(data) != IS_STRING)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"ip_addr\" is not string type, type=%d!", \
			__LINE__, ZEND_TYPE_OF(data));
		return EINVAL;
	}

	ip_addr = Z_STRVAL_P(data);
	ip_len = Z_STRLEN_P(data);
	if (ip_len >= IP_ADDRESS_SIZE)
	{
		ip_len = IP_ADDRESS_SIZE - 1;
	}
	memcpy(pTrackerServer->ip_addr, ip_addr, ip_len);

	if (zend_hash_find_wrapper(tracker_hash, "port", sizeof("port"), \
			&data) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"port\" not exist!", __LINE__);
		return ENOENT;
	}
	if (ZEND_TYPE_OF(data) != IS_LONG)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"port\" is not long type, type=%d!", \
			__LINE__, ZEND_TYPE_OF(data));
		return EINVAL;
	}
	pTrackerServer->port = data->value.lval;

	if (zend_hash_find_wrapper(tracker_hash, "sock", sizeof("sock"), \
			&data) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"sock\" not exist!", __LINE__);
		return ENOENT;
	}
	if (ZEND_TYPE_OF(data) != IS_LONG)
	{
		logError("file: "__FILE__", line: %d, " \
			"key \"sock\" is not long type, type=%d!", \
			__LINE__, ZEND_TYPE_OF(data));
		return EINVAL;
	}

	pTrackerServer->sock = data->value.lval;
	return 0;
}

static void php_fastdfs_active_test_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int argc;
	zval *server_info;
	HashTable *tracker_hash;
	ConnectionInfo server;

	argc = ZEND_NUM_ARGS();
	if (argc != 1)
	{
		logError("file: "__FILE__", line: %d, " \
			"fastdfs_active_test parameters count: %d != 1", \
			__LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", \
				&server_info) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_hash = Z_ARRVAL_P(server_info);

	if ((pContext->err_no=php_fdfs_get_server_from_hash(tracker_hash, \
		&server)) != 0)
	{
		RETURN_BOOL(false);
	}

	if ((pContext->err_no=fdfs_active_test(&server)) != 0)
	{
		RETURN_BOOL(false);
	}
	else
	{
		RETURN_BOOL(true);
	}
}

static void php_fdfs_tracker_list_groups_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int argc;
	char *group_name;
	zend_size_t group_nlen;
	zval *tracker_obj;
	zval *group_info_array;
	zval *server_info_array;
	HashTable *tracker_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo *pTrackerServer;
	FDFSGroupStat group_stats[FDFS_MAX_GROUPS];
	FDFSGroupStat *pGroupStat;
	FDFSGroupStat *pGroupEnd;
	int group_count;
	int result;
        int storage_count;
	int saved_tracker_sock;
	FDFSStorageInfo storage_infos[FDFS_MAX_SERVERS_EACH_GROUP];
	FDFSStorageInfo *pStorage;
	FDFSStorageInfo *pStorageEnd;
	FDFSStorageStat *pStorageStat;

	argc = ZEND_NUM_ARGS();
	if (argc > 2)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker_list_groups parameters count: %d > 2", 
			__LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	group_name = NULL;
	group_nlen = 0;
	tracker_obj = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sa", \
			&group_name, &group_nlen, &tracker_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}

		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		saved_tracker_sock = pTrackerServer->sock;
	}

	if (group_name != NULL && group_nlen > 0)
	{
		group_count = 1;
		result = tracker_list_one_group(pTrackerServer, group_name, \
				group_stats);
	}
	else
	{
		result = tracker_list_groups(pTrackerServer, group_stats, \
				FDFS_MAX_GROUPS, &group_count);
	}

	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}

	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		pContext->err_no = result;
		RETURN_BOOL(false);
	}

	pContext->err_no = 0;
	array_init(return_value);

	pGroupEnd = group_stats + group_count;
	for (pGroupStat=group_stats; pGroupStat<pGroupEnd; pGroupStat++)
	{

		ALLOC_INIT_ZVAL(group_info_array);
		array_init(group_info_array);

		zend_add_assoc_zval_ex(return_value, pGroupStat->group_name, \
			strlen(pGroupStat->group_name) + 1, group_info_array);

		zend_add_assoc_long_ex(group_info_array, "total_space", \
			sizeof("total_space"), pGroupStat->total_mb);
		zend_add_assoc_long_ex(group_info_array, "free_space", \
			sizeof("free_space"), pGroupStat->free_mb);
		zend_add_assoc_long_ex(group_info_array, "trunk_free_space", \
			sizeof("trunk_free_space"), pGroupStat->trunk_free_mb);
		zend_add_assoc_long_ex(group_info_array, "server_count", \
			sizeof("server_count"), pGroupStat->count);
		zend_add_assoc_long_ex(group_info_array, "active_count", \
			sizeof("active_count"), pGroupStat->active_count);
		zend_add_assoc_long_ex(group_info_array, "storage_port", \
			sizeof("storage_port"), pGroupStat->storage_port);
		zend_add_assoc_long_ex(group_info_array, "storage_http_port", \
			sizeof("storage_http_port"), \
			pGroupStat->storage_http_port);
		zend_add_assoc_long_ex(group_info_array, "store_path_count", \
			sizeof("store_path_count"), \
			pGroupStat->store_path_count);
		zend_add_assoc_long_ex(group_info_array, "subdir_count_per_path", \
			sizeof("subdir_count_per_path"), \
			pGroupStat->subdir_count_per_path);
		zend_add_assoc_long_ex(group_info_array, "current_write_server", \
			sizeof("current_write_server"), \
			pGroupStat->current_write_server);
		zend_add_assoc_long_ex(group_info_array, "current_trunk_file_id", \
			sizeof("current_trunk_file_id"), \
			pGroupStat->current_trunk_file_id);
		       
		result = tracker_list_servers(pTrackerServer, \
				pGroupStat->group_name, NULL, \
				storage_infos, FDFS_MAX_SERVERS_EACH_GROUP, \
				&storage_count);
		if (result != 0)
		{
			if (tracker_obj == NULL)
			{
				conn_pool_disconnect_server(pTrackerServer);
			}

			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		pStorageEnd = storage_infos + storage_count;
		for (pStorage=storage_infos; pStorage<pStorageEnd; \
				pStorage++)
		{
			ALLOC_INIT_ZVAL(server_info_array);
			array_init(server_info_array);

			zend_add_assoc_zval_ex(group_info_array, pStorage->id, \
				strlen(pStorage->id) + 1, server_info_array);

			zend_add_assoc_stringl_ex(server_info_array, \
				"ip_addr", sizeof("ip_addr"), \
				pStorage->ip_addr, strlen(pStorage->ip_addr), 1);

			zend_add_assoc_long_ex(server_info_array, \
				"join_time", sizeof("join_time"), \
				pStorage->join_time);

			zend_add_assoc_long_ex(server_info_array, \
				"up_time", sizeof("up_time"), \
				pStorage->up_time);

			zend_add_assoc_stringl_ex(server_info_array, \
				"http_domain", sizeof("http_domain"), \
				pStorage->domain_name, \
				strlen(pStorage->domain_name), 1);

			zend_add_assoc_stringl_ex(server_info_array, \
				"version", sizeof("version"), \
				pStorage->version, strlen(pStorage->version), 1);

			zend_add_assoc_stringl_ex(server_info_array, \
				"src_storage_id", sizeof("src_storage_id"), \
				pStorage->src_id, strlen(pStorage->src_id), 1);

			zend_add_assoc_bool_ex(server_info_array, \
				"if_trunk_server", sizeof("if_trunk_server"), \
				pStorage->if_trunk_server);

			zend_add_assoc_long_ex(server_info_array, \
				"upload_priority", sizeof("upload_priority"), \
				pStorage->upload_priority);

			zend_add_assoc_long_ex(server_info_array, \
				"store_path_count", sizeof("store_path_count"),\
				pStorage->store_path_count);

			zend_add_assoc_long_ex(server_info_array, \
				"subdir_count_per_path", \
				sizeof("subdir_count_per_path"), \
				pStorage->subdir_count_per_path);

			zend_add_assoc_long_ex(server_info_array, \
				"storage_port", sizeof("storage_port"), \
				pStorage->storage_port);

			zend_add_assoc_long_ex(server_info_array, \
				"storage_http_port", \
				sizeof("storage_http_port"), \
				pStorage->storage_http_port);

			zend_add_assoc_long_ex(server_info_array, \
				"current_write_path", \
				sizeof("current_write_path"), \
				pStorage->current_write_path);

			zend_add_assoc_long_ex(server_info_array, "status", \
				sizeof("status"), pStorage->status);
			zend_add_assoc_long_ex(server_info_array, "total_space", \
				sizeof("total_space"), pStorage->total_mb);
			zend_add_assoc_long_ex(server_info_array, "free_space", \
				sizeof("free_space"), pStorage->free_mb);

			pStorageStat = &(pStorage->stat);

			zend_add_assoc_long_ex(server_info_array, \
				"connection.alloc_count", \
				sizeof("connection.alloc_count"), \
				pStorageStat->connection.alloc_count);

			zend_add_assoc_long_ex(server_info_array, \
				"connection.current_count", \
				sizeof("connection.current_count"), \
				pStorageStat->connection.current_count);

			zend_add_assoc_long_ex(server_info_array, \
				"connection.max_count", \
				sizeof("connection.max_count"), \
				pStorageStat->connection.max_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_upload_count", \
				sizeof("total_upload_count"), \
				pStorageStat->total_upload_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_upload_count", \
				sizeof("success_upload_count"), \
				pStorageStat->success_upload_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_append_count", \
				sizeof("total_append_count"), \
				pStorageStat->total_append_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_append_count", \
				sizeof("success_append_count"), \
				pStorageStat->success_append_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_modify_count", \
				sizeof("total_modify_count"), \
				pStorageStat->total_modify_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_modify_count", \
				sizeof("success_modify_count"), \
				pStorageStat->success_modify_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_truncate_count", \
				sizeof("total_truncate_count"), \
				pStorageStat->total_truncate_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_truncate_count", \
				sizeof("success_truncate_count"), \
				pStorageStat->success_truncate_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_set_meta_count", \
				sizeof("total_set_meta_count"), \
				pStorageStat->total_set_meta_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_set_meta_count", \
				sizeof("success_set_meta_count"), \
				pStorageStat->success_set_meta_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_delete_count", \
				sizeof("total_delete_count"), \
				pStorageStat->total_delete_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_delete_count", \
				sizeof("success_delete_count"), \
				pStorageStat->success_delete_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_download_count", \
				sizeof("total_download_count"), \
				pStorageStat->total_download_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_download_count", \
				sizeof("success_download_count"), \
				pStorageStat->success_download_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_get_meta_count", \
				sizeof("total_get_meta_count"), \
				pStorageStat->total_get_meta_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_get_meta_count", \
				sizeof("success_get_meta_count"), \
				pStorageStat->success_get_meta_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_create_link_count", \
				sizeof("total_create_link_count"), \
				pStorageStat->total_create_link_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_create_link_count", \
				sizeof("success_create_link_count"), \
				pStorageStat->success_create_link_count);

			zend_add_assoc_long_ex(server_info_array, \
				"total_delete_link_count", \
				sizeof("total_delete_link_count"), \
				pStorageStat->total_delete_link_count);

			zend_add_assoc_long_ex(server_info_array, \
				"success_delete_link_count", \
				sizeof("success_delete_link_count"), \
				pStorageStat->success_delete_link_count);
			zend_add_assoc_long_ex(server_info_array, \
				"total_upload_bytes", \
				sizeof("total_upload_bytes"), \
				pStorageStat->total_upload_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"success_upload_bytes", \
				sizeof("success_upload_bytes"), \
				pStorageStat->success_upload_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"total_append_bytes", \
				sizeof("total_append_bytes"), \
				pStorageStat->total_append_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"success_append_bytes", \
				sizeof("success_append_bytes"), \
				pStorageStat->success_append_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"total_modify_bytes", \
				sizeof("total_modify_bytes"), \
				pStorageStat->total_modify_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"success_modify_bytes", \
				sizeof("success_modify_bytes"), \
				pStorageStat->success_modify_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"total_download_bytes", \
				sizeof("total_download_bytes"), \
				pStorageStat->total_download_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"success_download_bytes", \
				sizeof("success_download_bytes"), \
				pStorageStat->success_download_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"total_sync_in_bytes", \
				sizeof("total_sync_in_bytes"), \
				pStorageStat->total_sync_in_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"success_sync_in_bytes", \
				sizeof("success_sync_in_bytes"), \
				pStorageStat->success_sync_in_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"total_sync_out_bytes", \
				sizeof("total_sync_out_bytes"), \
				pStorageStat->total_sync_out_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"success_sync_out_bytes", \
				sizeof("success_sync_out_bytes"), \
				pStorageStat->success_sync_out_bytes);
			zend_add_assoc_long_ex(server_info_array, \
				"total_file_open_count", \
				sizeof("total_file_open_count"), \
				pStorageStat->total_file_open_count);
			zend_add_assoc_long_ex(server_info_array, \
				"success_file_open_count", \
				sizeof("success_file_open_count"), \
				pStorageStat->success_file_open_count);
			zend_add_assoc_long_ex(server_info_array, \
				"total_file_read_count", \
				sizeof("total_file_read_count"), \
				pStorageStat->total_file_read_count);
			zend_add_assoc_long_ex(server_info_array, \
				"success_file_read_count", \
				sizeof("success_file_read_count"), \
				pStorageStat->success_file_read_count);
			zend_add_assoc_long_ex(server_info_array, \
				"total_file_write_count", \
				sizeof("total_file_write_count"), \
				pStorageStat->total_file_write_count);
			zend_add_assoc_long_ex(server_info_array, \
				"success_file_write_count", \
				sizeof("success_file_write_count"), \
				pStorageStat->success_file_write_count);
			zend_add_assoc_long_ex(server_info_array, \
				"last_heart_beat_time", \
				sizeof("last_heart_beat_time"), \
				pStorageStat->last_heart_beat_time);

			zend_add_assoc_long_ex(server_info_array, \
				"last_source_update", \
				sizeof("last_source_update"), \
				pStorageStat->last_source_update);
			zend_add_assoc_long_ex(server_info_array, \
				"last_sync_update", \
				sizeof("last_sync_update"), \
				pStorageStat->last_sync_update);
			zend_add_assoc_long_ex(server_info_array, \
				"last_synced_timestamp", \
				sizeof("last_synced_timestamp"), \
				pStorageStat->last_synced_timestamp);
		}
	}
}

static void php_fdfs_tracker_query_storage_store_impl( \
		INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int argc;
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char *group_name;
	zend_size_t group_nlen;
	zval *tracker_obj;
	HashTable *tracker_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	int store_path_index;
	int saved_tracker_sock;
	int result;

    	argc = ZEND_NUM_ARGS();
	if (argc > 2)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker_query_storage_store parameters " \
			"count: %d > 2", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	group_name = NULL;
	group_nlen = 0;
	tracker_obj = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sa", \
			&group_name, &group_nlen, &tracker_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (group_name != NULL && group_nlen > 0)
	{
		snprintf(new_group_name, sizeof(new_group_name), "%s", group_name);
		result = tracker_query_storage_store_with_group(pTrackerServer,\
                	new_group_name, &storage_server, &store_path_index);
	}
	else
	{
		*new_group_name = '\0';
		result = tracker_query_storage_store_without_group( \
			pTrackerServer, &storage_server, new_group_name, \
			&store_path_index);
	}

	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}

	pContext->err_no = result;
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	array_init(return_value);
	zend_add_assoc_stringl_ex(return_value, "ip_addr", \
			sizeof("ip_addr"), storage_server.ip_addr, \
			strlen(storage_server.ip_addr), 1);
	zend_add_assoc_long_ex(return_value, "port", sizeof("port"), \
			storage_server.port);
	zend_add_assoc_long_ex(return_value, "sock", sizeof("sock"), -1);
	zend_add_assoc_long_ex(return_value, "store_path_index", \
			sizeof("store_path_index"), \
			store_path_index);
}

static void php_fdfs_tracker_query_storage_store_list_impl( \
		INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int argc;
	char *group_name;
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	zend_size_t group_nlen;
	zval *server_info_array;
	zval *tracker_obj;
	HashTable *tracker_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_servers[FDFS_MAX_SERVERS_EACH_GROUP];
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pServer;
	ConnectionInfo *pServerEnd;
	int store_path_index;
	int saved_tracker_sock;
	int result;
	int storage_count;

    	argc = ZEND_NUM_ARGS();
	if (argc > 2)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker_query_storage_store_list parameters " \
			"count: %d > 2", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	group_name = NULL;
	group_nlen = 0;
	tracker_obj = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sa", \
			&group_name, &group_nlen, &tracker_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (group_name != NULL && group_nlen > 0)
	{
		snprintf(new_group_name, sizeof(new_group_name), "%s", group_name);
		result = tracker_query_storage_store_list_with_group(pTrackerServer,\
                	new_group_name, storage_servers, FDFS_MAX_SERVERS_EACH_GROUP, \
			&storage_count, &store_path_index);
	}
	else
	{
		*new_group_name = '\0';
		result = tracker_query_storage_store_list_without_group( \
			pTrackerServer, storage_servers, \
			FDFS_MAX_SERVERS_EACH_GROUP, &storage_count, \
			new_group_name, &store_path_index);
	}

	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}

	pContext->err_no = result;
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	array_init(return_value);

	pServerEnd = storage_servers + storage_count;
	for (pServer=storage_servers; pServer<pServerEnd; pServer++)
	{
		ALLOC_INIT_ZVAL(server_info_array);
		array_init(server_info_array);

		add_index_zval(return_value, pServer - storage_servers, \
			server_info_array);

		zend_add_assoc_stringl_ex(server_info_array, "ip_addr", \
				sizeof("ip_addr"), pServer->ip_addr, \
				strlen(pServer->ip_addr), 1);
		zend_add_assoc_long_ex(server_info_array, "port", sizeof("port"), \
				pServer->port);
		zend_add_assoc_long_ex(server_info_array, "sock", sizeof("sock"), -1);
		zend_add_assoc_long_ex(server_info_array, "store_path_index", \
				sizeof("store_path_index"), \
				store_path_index);
	}
}

static void php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext, const byte cmd, \
		const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	zval *tracker_obj;
	HashTable *tracker_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	int result;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 1;
		max_param_count = 2;
	}
	else
	{
		min_param_count = 2;
		max_param_count = 3;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker_do_query_storage parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a", \
			&file_id, &file_id_len, &tracker_obj) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a", \
		&group_name, &group_nlen, &remote_filename, &filename_len, \
		&tracker_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	result = tracker_do_query_storage(pTrackerServer, &storage_server, \
			cmd, group_name, remote_filename);

	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}

	pContext->err_no = result;
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	array_init(return_value);
	zend_add_assoc_stringl_ex(return_value, "ip_addr", \
			sizeof("ip_addr"), storage_server.ip_addr, \
			strlen(storage_server.ip_addr), 1);
	zend_add_assoc_long_ex(return_value, "port", sizeof("port"), \
			storage_server.port);
	zend_add_assoc_long_ex(return_value, "sock", sizeof("sock"), -1);
}

static void php_fdfs_tracker_delete_storage_impl( \
		INTERNAL_FUNCTION_PARAMETERS, 
		FDFSPhpContext *pContext)
{
	int argc;
	zend_size_t group_name_len;
	zend_size_t storage_ip_len;
	char *group_name;
	char *storage_ip;

    	argc = ZEND_NUM_ARGS();
	if (argc != 2)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker_delete_storage parameters " \
			"count: %d != 2", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", \
		&group_name, &group_name_len, &storage_ip, &storage_ip_len)
		 == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (group_name_len == 0 || storage_ip_len == 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"group name length: %d or storage ip length: %d = 0!",\
			__LINE__, group_name_len, storage_ip_len);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	pContext->err_no = tracker_delete_storage(pContext->pTrackerGroup, \
                	group_name, storage_ip);
	if (pContext->err_no == 0)
	{
		RETURN_BOOL(true);
	}
	else
	{
		RETURN_BOOL(false);
	}
}

static void php_fdfs_storage_delete_file_impl( \
		INTERNAL_FUNCTION_PARAMETERS, 
		FDFSPhpContext *pContext, const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	zval *tracker_obj;
	zval *storage_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	int result;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	int saved_storage_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 1;
		max_param_count = 3;
	}
	else
	{
		min_param_count = 2;
		max_param_count = 4;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_delete_file parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|aa", \
			&file_id, &file_id_len, &tracker_obj, &storage_obj) \
			== FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|aa", \
		&group_name, &group_nlen, &remote_filename, &filename_len, \
		&tracker_obj, &storage_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	result = storage_delete_file(pTrackerServer, pStorageServer, \
			group_name, remote_filename);
	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	RETURN_BOOL(true);
}

static void php_fdfs_storage_truncate_file_impl( \
		INTERNAL_FUNCTION_PARAMETERS, 
		FDFSPhpContext *pContext, const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	zval *tracker_obj;
	zval *storage_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	int result;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	int saved_storage_sock;
	long truncated_file_size = 0;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 1;
		max_param_count = 4;
	}
	else
	{
		min_param_count = 2;
		max_param_count = 5;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_truncate_file parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"s|laa", &file_id, &file_id_len, \
			&truncated_file_size, &tracker_obj, \
			&storage_obj) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
		"ss|laa", &group_name, &group_nlen, &remote_filename, \
		&filename_len, &truncated_file_size, &tracker_obj, \
		&storage_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	result = storage_truncate_file(pTrackerServer, pStorageServer, \
			group_name, remote_filename, truncated_file_size);
	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	RETURN_BOOL(true);
}

static void php_fdfs_storage_download_file_to_callback_impl( \
	INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext, \
	const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	zval *download_callback;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	long file_offset;
	long download_bytes;
	int64_t file_size;
	zval *tracker_obj;
	zval *storage_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	HashTable *callback_hash;
	php_fdfs_callback_t php_callback;
	int result;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	int saved_storage_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 2;
		max_param_count = 6;
	}
	else
	{
		min_param_count = 3;
		max_param_count = 7;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_download_file_to_buff parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	file_offset = 0;
	download_bytes = 0;
	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"sa|llaa", &file_id, &file_id_len, \
			&download_callback, &file_offset, &download_bytes, \
			&tracker_obj, &storage_obj) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssa|llaa", \
		&group_name, &group_nlen, &remote_filename, &filename_len, \
		&download_callback, &file_offset, &download_bytes, \
		&tracker_obj, &storage_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	callback_hash = Z_ARRVAL_P(download_callback);
	result = php_fdfs_get_callback_from_hash(callback_hash, \
				&php_callback);
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		pContext->err_no = result;
		RETURN_BOOL(false);
	}

	result = storage_download_file_ex(pTrackerServer, pStorageServer, \
		group_name, remote_filename, file_offset, download_bytes, \
		php_fdfs_download_callback, (void *)&php_callback, &file_size);
	if (tracker_hash != NULL && pTrackerServer->sock != saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		pContext->err_no = result;
		RETURN_BOOL(false);
	}
	RETURN_BOOL(true);
}

static void php_fdfs_storage_download_file_to_buff_impl( \
	INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext, \
	const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	char *file_buff;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	long file_offset;
	long download_bytes;
	int64_t file_size;
	zval *tracker_obj;
	zval *storage_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	int result;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	int saved_storage_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 1;
		max_param_count = 5;
	}
	else
	{
		min_param_count = 2;
		max_param_count = 6;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_download_file_to_buff parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	file_offset = 0;
	download_bytes = 0;
	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|llaa", \
			&file_id, &file_id_len, &file_offset, &download_bytes, \
			&tracker_obj, &storage_obj) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|llaa", \
		&group_name, &group_nlen, &remote_filename, &filename_len, \
		&file_offset, &download_bytes, &tracker_obj, &storage_obj) \
		== FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	result=storage_do_download_file_ex(pTrackerServer, pStorageServer, \
		FDFS_DOWNLOAD_TO_BUFF, group_name, remote_filename, \
		file_offset, download_bytes, &file_buff, NULL, &file_size);
	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		pContext->err_no = result;
		RETURN_BOOL(false);
	}

	pContext->err_no = 0;
	ZEND_RETURN_STRINGL_CALLBACK(file_buff, file_size, free);
}

static void php_fdfs_storage_download_file_to_file_impl( \
	INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext, \
	const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	char *local_filename;
	zend_size_t group_nlen;
	zend_size_t remote_file_nlen;
	zend_size_t local_file_nlen;
	long file_offset;
	long download_bytes;
	int64_t file_size;
	zval *tracker_obj;
	zval *storage_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	int result;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	int saved_storage_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 2;
		max_param_count = 6;
	}
	else
	{
		min_param_count = 3;
		max_param_count = 7;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_download_file_to_file parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	file_offset = 0;
	download_bytes = 0;
	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|llaa",\
			&file_id, &file_id_len, &local_filename, \
			&local_file_nlen, &file_offset, &download_bytes, \
			&tracker_obj, &storage_obj) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss|llaa", \
		&group_name, &group_nlen, &remote_filename, &remote_file_nlen,\
		&local_filename, &local_file_nlen, &file_offset, \
		&download_bytes, &tracker_obj, &storage_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	result=storage_do_download_file_ex(pTrackerServer, pStorageServer, \
		FDFS_DOWNLOAD_TO_FILE, group_name, remote_filename, \
		file_offset, download_bytes, &local_filename, NULL, &file_size);
	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}
	else
	{
		RETURN_BOOL(true);
	}
}

static void php_fdfs_storage_get_metadata_impl( \
	INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext, \
	const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	zval *tracker_obj;
	zval *storage_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	FDFSMetaData *meta_list;
	FDFSMetaData *pMetaData;
	FDFSMetaData *pMetaEnd;
	int meta_count;
	int result;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	int saved_storage_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 1;
		max_param_count = 3;
	}
	else
	{
		min_param_count = 2;
		max_param_count = 4;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_get_metadata parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|aa", \
			&file_id, &file_id_len, &tracker_obj, &storage_obj) \
			== FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|aa", \
		&group_name, &group_nlen, &remote_filename, &filename_len, \
		&tracker_obj, &storage_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	result = storage_get_metadata(pTrackerServer, pStorageServer, \
		group_name, remote_filename, &meta_list, &meta_count);
	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	array_init(return_value);
	if (meta_list != NULL)
	{
		pMetaEnd = meta_list + meta_count;
		for (pMetaData=meta_list; pMetaData<pMetaEnd; pMetaData++)
		{
			zend_add_assoc_stringl_ex(return_value, pMetaData->name, \
				strlen(pMetaData->name)+1, pMetaData->value,\
				strlen(pMetaData->value), 1);
		}

		free(meta_list);
	}
}

static void php_fdfs_storage_file_exist_impl( \
	INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext, \
	const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	zval *tracker_obj;
	zval *storage_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	int result;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	int saved_storage_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 1;
		max_param_count = 3;
	}
	else
	{
		min_param_count = 2;
		max_param_count = 4;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_file_exist parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|aa", \
			&file_id, &file_id_len, &tracker_obj, &storage_obj) \
			== FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|aa", \
		&group_name, &group_nlen, &remote_filename, &filename_len, \
		&tracker_obj, &storage_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	result = storage_file_exist(pTrackerServer, pStorageServer, \
		group_name, remote_filename);
	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (result == 0)
	{
		RETURN_BOOL(true);
	}
	else
	{
		RETURN_BOOL(false);
	}
}

static void php_fdfs_tracker_query_storage_list_impl( \
		INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext, const bool bFileId)
{
	int argc;
	char *group_name;
	char *remote_filename;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	zval *tracker_obj;
	zval *server_info_array;
	HashTable *tracker_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_servers[FDFS_MAX_SERVERS_EACH_GROUP];
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pServer;
	ConnectionInfo *pServerEnd;
	int result;
	int server_count;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 1;
		max_param_count = 2;
	}
	else
	{
		min_param_count = 2;
		max_param_count = 3;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"fastdfs_tracker_query_storage_list parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a", \
			&file_id, &file_id_len, &tracker_obj) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a", \
		&group_name, &group_nlen, &remote_filename, &filename_len, \
		&tracker_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	result = tracker_query_storage_list(pTrackerServer, storage_servers, \
			FDFS_MAX_SERVERS_EACH_GROUP, &server_count, \
			group_name, remote_filename);
	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	pContext->err_no = result;
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	array_init(return_value);

	pServerEnd = storage_servers + server_count;
	for (pServer=storage_servers; pServer<pServerEnd; pServer++)
	{
		ALLOC_INIT_ZVAL(server_info_array);
		array_init(server_info_array);

		add_index_zval(return_value, pServer - storage_servers, \
			server_info_array);

		zend_add_assoc_stringl_ex(server_info_array, "ip_addr", \
			sizeof("ip_addr"), pServer->ip_addr, \
			strlen(pServer->ip_addr), 1);
		zend_add_assoc_long_ex(server_info_array, "port", sizeof("port"), \
			pServer->port);
		zend_add_assoc_long_ex(server_info_array,"sock",sizeof("sock"),-1);
	}
}

static int php_fdfs_upload_callback(void *arg, const int64_t file_size, int sock)
{
	php_fdfs_upload_callback_t *pUploadCallback;
	zval *args[2];
	zval zsock;
	zval ret;
	zval null_args;
	int result;
	TSRMLS_FETCH();

	INIT_ZVAL(ret);
	ZVAL_NULL(&ret);

	INIT_ZVAL(zsock);
	ZVAL_LONG(&zsock, sock);

	pUploadCallback = (php_fdfs_upload_callback_t *)arg;
	if (pUploadCallback->callback.args == NULL)
	{
		INIT_ZVAL(null_args);
		ZVAL_NULL(&null_args);
		pUploadCallback->callback.args = &null_args;
	}
	args[0] = &zsock;
	args[1] = pUploadCallback->callback.args;

	if (zend_call_user_function_wrapper(EG(function_table), NULL, \
		pUploadCallback->callback.func_name, 
		&ret, 2, args TSRMLS_CC) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"call callback function: %s fail", __LINE__, \
			Z_STRVAL_P(pUploadCallback->callback.func_name));
		return EINVAL;
	}

	if (ZEND_TYPE_OF(&ret) == IS_LONG)
	{
		result = ret.value.lval != 0 ? 0 : EFAULT;
	}
	else if (ZEND_IS_BOOL(&ret))
	{
		result = ZEND_IS_TRUE(&ret) ? 0 : EFAULT;
	}
	else
	{
		logError("file: "__FILE__", line: %d, " \
			"callback function return invalid value type: %d", \
			__LINE__, ZEND_TYPE_OF(&ret));
		result = EINVAL;
	}

	return result;
}

static int php_fdfs_download_callback(void *arg, const int64_t file_size, \
		const char *data, const int current_size)
{
	php_fdfs_callback_t *pCallback;
	zval *args[3];
	zval zfilesize;
	zval zdata;
	zval ret;
	zval null_args;
	int result;
#if PHP_MAJOR_VERSION >= 7
    zend_string *sz_data;
    bool use_heap_data;
#endif

	TSRMLS_FETCH();

	INIT_ZVAL(ret);
	ZVAL_NULL(&ret);

	INIT_ZVAL(zfilesize);
	ZVAL_LONG(&zfilesize, file_size);

#if PHP_MAJOR_VERSION < 7
	INIT_ZVAL(zdata);
	ZVAL_STRINGL(&zdata, (char *)data, current_size, 0);
#else
    ZSTR_ALLOCA_INIT(sz_data, (char *)data, current_size, use_heap_data);
    ZVAL_NEW_STR(&zdata, sz_data);
#endif

	pCallback = (php_fdfs_callback_t *)arg;
	if (pCallback->args == NULL)
	{
		INIT_ZVAL(null_args);
		ZVAL_NULL(&null_args);
		pCallback->args = &null_args;
	}
	args[0] = pCallback->args;
	args[1] = &zfilesize;
	args[2] = &zdata;
	if (zend_call_user_function_wrapper(EG(function_table), NULL, \
		pCallback->func_name, 
		&ret, 3, args TSRMLS_CC) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"call callback function: %s fail", __LINE__, \
			Z_STRVAL_P(pCallback->func_name));
		return EINVAL;
	}

	if (ZEND_TYPE_OF(&ret) == IS_LONG)
	{
		result = ret.value.lval != 0 ? 0 : EFAULT;
	}
	else if (ZEND_IS_BOOL(&ret))
	{
		result = ZEND_IS_TRUE(&ret) ? 0 : EFAULT;
	}
	else
	{
		logError("file: "__FILE__", line: %d, " \
			"callback function return invalid value type: %d", \
			__LINE__, ZEND_TYPE_OF(&ret));
		result = EINVAL;
	}

#if PHP_MAJOR_VERSION >= 7
    ZSTR_ALLOCA_FREE(sz_data, use_heap_data);
#endif

	return result;
}

/*
string/array fastdfs_storage_upload_by_filename(string local_filename
	[, string file_ext_name, array meta_list, string group_name, 
	array tracker_server, array storage_server])
return string/array for success, false for error
*/
static void php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext, const byte cmd, \
		const byte upload_type, const bool bFileId)
{
	int result;
	int argc;
	char *local_filename;
	zend_size_t filename_len;
	char *file_ext_name;
	zval *callback_obj;
	zval *ext_name_obj;
	zval *metadata_obj;
	zval *tracker_obj;
	zval *storage_obj;
	zval *group_name_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	FDFSMetaData *meta_list;
	int meta_count;
	int store_path_index;
	int saved_tracker_sock;
	int saved_storage_sock;
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[128];

    	argc = ZEND_NUM_ARGS();
	if (argc < 1 || argc > 6)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_upload_file parameters " \
			"count: %d < 1 or > 6", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	local_filename = NULL;
	filename_len = 0;
	callback_obj = NULL;
	ext_name_obj = NULL;
	metadata_obj = NULL;
	group_name_obj = NULL;
	tracker_obj = NULL;
	storage_obj = NULL;
	if (upload_type == FDFS_UPLOAD_BY_CALLBACK)
	{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"a|zazaa", &callback_obj, &ext_name_obj, \
			&metadata_obj, &group_name_obj, &tracker_obj, \
			&storage_obj);
	}
	else
	{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"s|zazaa", &local_filename, &filename_len, \
			&ext_name_obj, &metadata_obj, &group_name_obj, \
			&tracker_obj, &storage_obj);
	}

	if (result == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (ext_name_obj == NULL)
	{
		file_ext_name = NULL;
	}
	else
	{
		if (ZEND_TYPE_OF(ext_name_obj) == IS_NULL)
		{
			file_ext_name = NULL;
		}
		else if (ZEND_TYPE_OF(ext_name_obj) == IS_STRING)
		{
			file_ext_name = Z_STRVAL_P(ext_name_obj);
		}
		else
		{
			logError("file: "__FILE__", line: %d, " \
				"file_ext_name is not a string, type=%d!", \
				__LINE__, ZEND_TYPE_OF(ext_name_obj));
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}
	}

	if (group_name_obj != NULL && ZEND_TYPE_OF(group_name_obj) == IS_STRING)
	{
		snprintf(group_name, sizeof(group_name), "%s", \
			Z_STRVAL_P(group_name_obj));
	}
	else
	{
		*group_name = '\0';
	}
	*remote_filename = '\0';

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		store_path_index = 0;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		zval *data;

		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		if (zend_hash_find_wrapper(storage_hash, "store_path_index", \
			sizeof("store_path_index"), &data) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"key \"store_path_index\" not exist!", \
				__LINE__);
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}

		if (ZEND_TYPE_OF(data) == IS_LONG)
		{
			store_path_index = data->value.lval;
		}
		else if (ZEND_TYPE_OF(data) == IS_STRING)
		{
			store_path_index = atoi(Z_STRVAL_P(data));
		}
		else
		{
			logError("file: "__FILE__", line: %d, " \
				"key \"store_path_index\" is invalid, " \
				"type=%d!", __LINE__, ZEND_TYPE_OF(data));
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	if (metadata_obj == NULL)
	{
		meta_list = NULL;
		meta_count = 0;
	}
	else
	{
		result = fastdfs_convert_metadata_to_array(metadata_obj, \
				&meta_list, &meta_count);
		if (result != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
	}

	if (upload_type == FDFS_UPLOAD_BY_FILE)
	{
	result = storage_upload_by_filename_ex(pTrackerServer, pStorageServer, \
			store_path_index, cmd, local_filename, file_ext_name, \
			meta_list, meta_count, group_name, remote_filename);
	}
	else if (upload_type == FDFS_UPLOAD_BY_BUFF)
	{
	char *buff;
	int buff_len;

	buff = local_filename;
	buff_len = filename_len;

	result = storage_do_upload_file(pTrackerServer, pStorageServer, \
                store_path_index, cmd, FDFS_UPLOAD_BY_BUFF, buff, NULL, \
                buff_len, NULL, NULL, file_ext_name, meta_list, meta_count, \
                group_name, remote_filename);
	}
	else  //by callback
	{
		HashTable *callback_hash;
		php_fdfs_upload_callback_t php_callback;

		callback_hash = Z_ARRVAL_P(callback_obj);
		result = php_fdfs_get_upload_callback_from_hash(callback_hash, \
				&php_callback);
		if (result != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		result = storage_upload_by_callback_ex(pTrackerServer, \
			pStorageServer, store_path_index, cmd, \
			php_fdfs_upload_callback, (void *)&php_callback, \
                	php_callback.file_size, file_ext_name, meta_list, \
			meta_count, group_name, remote_filename);
	}

	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (meta_list != NULL)
	{
		free(meta_list);
	}
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	if (bFileId)
	{
		char file_id[FDFS_GROUP_NAME_MAX_LEN + 128];
		int file_id_len;

		file_id_len = sprintf(file_id, "%s%c%s", group_name, \
				FDFS_FILE_ID_SEPERATOR, remote_filename);
		ZEND_RETURN_STRINGL(file_id, file_id_len, 1);
	}
	else
	{
		array_init(return_value);

		zend_add_assoc_stringl_ex(return_value, "group_name", \
			sizeof("group_name"), group_name, \
			strlen(group_name), 1);
		zend_add_assoc_stringl_ex(return_value, "filename", \
			sizeof("filename"), remote_filename, \
			strlen(remote_filename), 1);
	}
}

static void php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext, \
		const byte upload_type, const bool bFileId)
{
	int result;
	int argc;
	zval *callback_obj;
	char *local_filename;
	char *master_filename;
	char *prefix_name;
	char *file_ext_name;
	zval *ext_name_obj;
	zval *metadata_obj;
	zval *tracker_obj;
	zval *storage_obj;
	char *group_name;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	FDFSMetaData *meta_list;
	int meta_count;
	zend_size_t filename_len;
	zend_size_t group_name_len;
	zend_size_t master_filename_len;
	zend_size_t prefix_name_len;
	int saved_tracker_sock;
	int saved_storage_sock;
	int min_param_count;
	int max_param_count;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[128];

	if (bFileId)
	{
		min_param_count = 3;
		max_param_count = 7;
	}
	else
	{
		min_param_count = 4;
		max_param_count = 8;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_upload_slave_file parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	local_filename = NULL;
	filename_len = 0;
	callback_obj = NULL;
	ext_name_obj = NULL;
	metadata_obj = NULL;
	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *master_file_id;
		zend_size_t master_file_id_len;

		if (upload_type == FDFS_UPLOAD_BY_CALLBACK)
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"ass|zaaa", &callback_obj, \
			&master_file_id, &master_file_id_len, \
			&prefix_name, &prefix_name_len, &ext_name_obj, \
			&metadata_obj, &tracker_obj, &storage_obj);
		}
		else
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"sss|zaaa", &local_filename, &filename_len, \
			&master_file_id, &master_file_id_len, \
			&prefix_name, &prefix_name_len, &ext_name_obj, \
			&metadata_obj, &tracker_obj, &storage_obj);
		}
		if (result == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", master_file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"master_file_id is invalid, master_file_id=%s",\
				__LINE__, master_file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		master_filename =  pSeperator + 1;
	}
	else
	{
		if (upload_type == FDFS_UPLOAD_BY_CALLBACK)
		{
		 result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
				"asss|zaaa", &callback_obj, \
				&group_name, &group_name_len, \
				&master_filename, &master_filename_len, \
				&prefix_name, &prefix_name_len, &ext_name_obj,\
				&metadata_obj, &tracker_obj, &storage_obj);
		}
		else
		{
		 result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
				"ssss|zaaa", &local_filename, &filename_len, \
				&group_name, &group_name_len, \
				&master_filename, &master_filename_len, \
				&prefix_name, &prefix_name_len, &ext_name_obj,\
				&metadata_obj, &tracker_obj, &storage_obj);
		}
		if (result == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}
	}

	if (ext_name_obj == NULL)
	{
		file_ext_name = NULL;
	}
	else
	{
		if (ZEND_TYPE_OF(ext_name_obj) == IS_NULL)
		{
			file_ext_name = NULL;
		}
		else if (ZEND_TYPE_OF(ext_name_obj) == IS_STRING)
		{
			file_ext_name = Z_STRVAL_P(ext_name_obj);
		}
		else
		{
			logError("file: "__FILE__", line: %d, " \
				"file_ext_name is not a string, type=%d!", \
				__LINE__, ZEND_TYPE_OF(ext_name_obj));
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}
	}

	*remote_filename = '\0';
	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		saved_storage_sock = pStorageServer->sock;
	}

	if (metadata_obj == NULL)
	{
		meta_list = NULL;
		meta_count = 0;
	}
	else
	{
		result = fastdfs_convert_metadata_to_array(metadata_obj, \
				&meta_list, &meta_count);
		if (result != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
	}

	snprintf(new_group_name, sizeof(new_group_name), "%s", group_name);
	if (upload_type == FDFS_UPLOAD_BY_FILE)
	{
	result = storage_upload_slave_by_filename(pTrackerServer, \
			pStorageServer, local_filename, master_filename, \
			prefix_name, file_ext_name, meta_list, meta_count, \
			new_group_name, remote_filename);
	}
	else if (upload_type == FDFS_UPLOAD_BY_BUFF)
	{
	result = storage_upload_slave_by_filebuff(pTrackerServer, \
			pStorageServer, local_filename, filename_len, \
			master_filename, prefix_name, file_ext_name, \
			meta_list, meta_count, new_group_name, remote_filename);
	}
	else  //by callback
	{
		HashTable *callback_hash;
		php_fdfs_upload_callback_t php_callback;

		callback_hash = Z_ARRVAL_P(callback_obj);
		result = php_fdfs_get_upload_callback_from_hash(callback_hash, \
				&php_callback);
		if (result != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		result = storage_upload_slave_by_callback(pTrackerServer, \
			pStorageServer, php_fdfs_upload_callback, \
			(void *)&php_callback, php_callback.file_size, \
			master_filename, prefix_name, file_ext_name, meta_list,\
			meta_count, new_group_name, remote_filename);
	}

	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (meta_list != NULL)
	{
		free(meta_list);
	}
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}

	if (bFileId)
	{
		char file_id[FDFS_GROUP_NAME_MAX_LEN + 128];
		int file_id_len;

		file_id_len = sprintf(file_id, "%s%c%s", new_group_name, \
				FDFS_FILE_ID_SEPERATOR, remote_filename);
		ZEND_RETURN_STRINGL(file_id, file_id_len, 1);
	}
	else
	{
		array_init(return_value);

		zend_add_assoc_stringl_ex(return_value, "group_name", \
			sizeof("group_name"), new_group_name, \
			strlen(new_group_name), 1);
		zend_add_assoc_stringl_ex(return_value, "filename", \
			sizeof("filename"), remote_filename, \
			strlen(remote_filename), 1);
	}
}

/*
boolean fastdfs_storage_append_by_filename(string local_filename, 
	string group_name, appender_filename, 
	[array tracker_server, array storage_server])
return true for success, false for error
*/
static void php_fdfs_storage_append_file_impl( \
		INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext, \
		const byte upload_type, const bool bFileId)
{
	int result;
	int argc;
	zval *callback_obj;
	char *local_filename;
	char *appender_filename;
	zval *tracker_obj;
	zval *storage_obj;
	char *group_name;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];
	zend_size_t filename_len;
	zend_size_t group_name_len;
	zend_size_t appender_filename_len;
	int saved_tracker_sock;
	int saved_storage_sock;
	int min_param_count;
	int max_param_count;

	if (bFileId)
	{
		min_param_count = 2;
		max_param_count = 4;
	}
	else
	{
		min_param_count = 3;
		max_param_count = 5;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_append_file parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	local_filename = NULL;
	filename_len = 0;
	callback_obj = NULL;
	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *appender_file_id;
		zend_size_t appender_file_id_len;

		if (upload_type == FDFS_UPLOAD_BY_CALLBACK)
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"as|aa", &callback_obj, &appender_file_id, \
			&appender_file_id_len, &tracker_obj, &storage_obj);
		}
		else
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"ss|aa", &local_filename, &filename_len, \
			&appender_file_id, &appender_file_id_len, \
			&tracker_obj, &storage_obj);
		}
		if (result == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", appender_file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"appender_file_id is invalid, " \
				"appender_file_id=%s", \
				__LINE__, appender_file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		appender_filename =  pSeperator + 1;
	}
	else
	{
		if (upload_type == FDFS_UPLOAD_BY_CALLBACK)
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"ass|aa", &callback_obj, &group_name, &group_name_len, \
			&appender_filename, &appender_filename_len, \
			&tracker_obj, &storage_obj);
		}
		else
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"sss|aa", &local_filename, &filename_len, \
			&group_name, &group_name_len, \
			&appender_filename, &appender_filename_len, \
			&tracker_obj, &storage_obj);
		}
		if (result == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		saved_storage_sock = pStorageServer->sock;
	}

	if (upload_type == FDFS_UPLOAD_BY_FILE)
	{
	result = storage_append_by_filename(pTrackerServer, \
			pStorageServer, local_filename, group_name, \
			appender_filename);
	}
	else if (upload_type == FDFS_UPLOAD_BY_BUFF)
	{
	result = storage_append_by_filebuff(pTrackerServer, \
			pStorageServer, local_filename, filename_len, \
			group_name, appender_filename);
	}
	else
	{
		HashTable *callback_hash;
		php_fdfs_upload_callback_t php_callback;

		callback_hash = Z_ARRVAL_P(callback_obj);
		result = php_fdfs_get_upload_callback_from_hash(callback_hash, \
				&php_callback);
		if (result != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		result = storage_append_by_callback(pTrackerServer, \
			pStorageServer, php_fdfs_upload_callback, \
			(void *)&php_callback, php_callback.file_size, \
			group_name, appender_filename);
	}

	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (result == 0)
	{
		RETURN_BOOL(true);
	}
	else
	{
		RETURN_BOOL(false);
	}
}

/*
boolean fastdfs_storage_modify_by_filename(string local_filename, 
	long file_offset, string group_name, appender_filename, 
	[array tracker_server, array storage_server])
return true for success, false for error
*/
static void php_fdfs_storage_modify_file_impl( \
		INTERNAL_FUNCTION_PARAMETERS, FDFSPhpContext *pContext, \
		const byte upload_type, const bool bFileId)
{
	int result;
	int argc;
	zval *callback_obj;
	char *local_filename;
	char *appender_filename;
	zval *tracker_obj;
	zval *storage_obj;
	char *group_name;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];
	zend_size_t filename_len;
	zend_size_t group_name_len;
	zend_size_t appender_filename_len;
	int saved_tracker_sock;
	int saved_storage_sock;
	int min_param_count;
	int max_param_count;
	long file_offset = 0;

	if (bFileId)
	{
		min_param_count = 3;
		max_param_count = 5;
	}
	else
	{
		min_param_count = 4;
		max_param_count = 6;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_modify_file parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	local_filename = NULL;
	filename_len = 0;
	callback_obj = NULL;
	tracker_obj = NULL;
	storage_obj = NULL;
	if (bFileId)
	{
		char *pSeperator;
		char *appender_file_id;
		zend_size_t appender_file_id_len;

		if (upload_type == FDFS_UPLOAD_BY_CALLBACK)
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"als|aa", &callback_obj, &file_offset, \
			&appender_file_id, &appender_file_id_len, \
			&tracker_obj, &storage_obj);
		}
		else
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"sls|aa", &local_filename, &filename_len, \
			&file_offset, &appender_file_id, &appender_file_id_len, \
			&tracker_obj, &storage_obj);
		}
		if (result == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", appender_file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"appender_file_id is invalid, " \
				"appender_file_id=%s", \
				__LINE__, appender_file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		appender_filename =  pSeperator + 1;
	}
	else
	{
		if (upload_type == FDFS_UPLOAD_BY_CALLBACK)
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"alss|aa", &callback_obj, &file_offset, \
			&group_name, &group_name_len, \
			&appender_filename, &appender_filename_len, \
			&tracker_obj, &storage_obj);
		}
		else
		{
		result = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, \
			"slss|aa", &local_filename, &filename_len, \
			&file_offset, &group_name, &group_name_len, \
			&appender_filename, &appender_filename_len, \
			&tracker_obj, &storage_obj);
		}
		if (result == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		saved_storage_sock = pStorageServer->sock;
	}

	if (upload_type == FDFS_UPLOAD_BY_FILE)
	{
	result = storage_modify_by_filename(pTrackerServer, \
			pStorageServer, local_filename, file_offset, \
			group_name, appender_filename);
	}
	else if (upload_type == FDFS_UPLOAD_BY_BUFF)
	{
	result = storage_modify_by_filebuff(pTrackerServer, \
			pStorageServer, local_filename, \
			file_offset, filename_len, \
			group_name, appender_filename);
	}
	else
	{
		HashTable *callback_hash;
		php_fdfs_upload_callback_t php_callback;

		callback_hash = Z_ARRVAL_P(callback_obj);
		result = php_fdfs_get_upload_callback_from_hash( \
				callback_hash, &php_callback);
		if (result != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}

		result = storage_modify_by_callback(pTrackerServer, \
			pStorageServer, php_fdfs_upload_callback, \
			(void *)&php_callback, file_offset, \
			php_callback.file_size, group_name, \
			appender_filename);
	}

	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (result == 0)
	{
		RETURN_BOOL(true);
	}
	else
	{
		RETURN_BOOL(false);
	}
}

static void php_fdfs_storage_set_metadata_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext, const bool bFileId)
{
	int result;
	int argc;
	char *group_name;
	char *remote_filename;
	char *op_type_str;
	char op_type;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	zend_size_t op_type_len;
	zval *metadata_obj;
	zval *tracker_obj;
	zval *storage_obj;
	HashTable *tracker_hash;
	HashTable *storage_hash;
	ConnectionInfo tracker_server;
	ConnectionInfo storage_server;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	FDFSMetaData *meta_list;
	int meta_count;
	int min_param_count;
	int max_param_count;
	int saved_tracker_sock;
	int saved_storage_sock;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		min_param_count = 1;
		max_param_count = 5;
	}
	else
	{
		min_param_count = 2;
		max_param_count = 6;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc < min_param_count || argc > max_param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_set_metadata parameters " \
			"count: %d < %d or > %d", __LINE__, argc, \
			min_param_count, max_param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	tracker_obj = NULL;
	storage_obj = NULL;
	op_type_str = NULL;
	op_type_len = 0;
	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|saa", \
			&file_id, &file_id_len, &metadata_obj, &op_type_str, \
			&op_type_len, &tracker_obj, &storage_obj) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssa|saa", \
		&group_name, &group_nlen, &remote_filename, &filename_len, \
		&metadata_obj, &op_type_str, &op_type_len, &tracker_obj, \
		&storage_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (tracker_obj == NULL)
	{
		pTrackerServer = tracker_get_connection_no_pool( \
					pContext->pTrackerGroup);
		if (pTrackerServer == NULL)
		{
			pContext->err_no = ENOENT;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = -1;
		tracker_hash = NULL;
	}
	else
	{
		pTrackerServer = &tracker_server;
		tracker_hash = Z_ARRVAL_P(tracker_obj);
		if ((result=php_fdfs_get_server_from_hash(tracker_hash, \
				pTrackerServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_tracker_sock = pTrackerServer->sock;
	}

	if (storage_obj == NULL)
	{
		pStorageServer = NULL;
		storage_hash = NULL;
		saved_storage_sock = -1;
	}
	else
	{
		pStorageServer = &storage_server;
		storage_hash = Z_ARRVAL_P(storage_obj);
		if ((result=php_fdfs_get_server_from_hash(storage_hash, \
				pStorageServer)) != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
		saved_storage_sock = pStorageServer->sock;
	}

	if (metadata_obj == NULL)
	{
		meta_list = NULL;
		meta_count = 0;
	}
	else
	{
		result = fastdfs_convert_metadata_to_array(metadata_obj, \
				&meta_list, &meta_count);
		if (result != 0)
		{
			pContext->err_no = result;
			RETURN_BOOL(false);
		}
	}

	if (op_type_str == NULL)
	{
		op_type = STORAGE_SET_METADATA_FLAG_MERGE;
	}
	else if (TO_UPPERCASE(*op_type_str) == STORAGE_SET_METADATA_FLAG_MERGE)
	{
		op_type = STORAGE_SET_METADATA_FLAG_MERGE;
	}
	else if (TO_UPPERCASE(*op_type_str) == STORAGE_SET_METADATA_FLAG_OVERWRITE)
	{
		op_type = STORAGE_SET_METADATA_FLAG_OVERWRITE;
	}
	else
	{
		logError("file: "__FILE__", line: %d, " \
			"invalid op_type: %s!", __LINE__, op_type_str);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	result = storage_set_metadata(pTrackerServer, pStorageServer, \
			group_name, remote_filename, \
			meta_list, meta_count, op_type);
	if (tracker_hash != NULL && pTrackerServer->sock != \
		saved_tracker_sock)
	{
		CLEAR_HASH_SOCK_FIELD(tracker_hash)
	}
	if (pStorageServer != NULL && pStorageServer->sock != \
		saved_storage_sock)
	{
		CLEAR_HASH_SOCK_FIELD(storage_hash)
	}

	pContext->err_no = result;
	if (meta_list != NULL)
	{
		free(meta_list);
	}
	if (result != 0)
	{
		if (tracker_obj == NULL)
		{
			conn_pool_disconnect_server(pTrackerServer);
		}
		RETURN_BOOL(false);
	}
	else
	{
		RETURN_BOOL(true);
	}
}

static void php_fdfs_http_gen_token_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int result;
	int argc;
	char *file_id;
	zend_size_t file_id_len;
	long ts;
	char token[64];

    	argc = ZEND_NUM_ARGS();
	if (argc != 2)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_upload_file parameters " \
			"count: %d != 2", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", \
		&file_id, &file_id_len, &ts) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	result = fdfs_http_gen_token(&g_anti_steal_secret_key, file_id, \
		(int)ts, token);
	pContext->err_no = result;
	if (result != 0)
	{
		RETURN_BOOL(false);
	}

	ZEND_RETURN_STRINGL(token, strlen(token), 1);
}

static void php_fdfs_send_data_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int argc;
	long sock;
	char *buff;
	zend_size_t buff_len;

    	argc = ZEND_NUM_ARGS();
	if (argc != 2)
	{
		logError("file: "__FILE__", line: %d, " \
			"send_data parameters " \
			"count: %d != 2", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", \
		&sock, &buff, &buff_len) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if ((pContext->err_no=tcpsenddata_nb(sock, buff, \
                        buff_len, g_fdfs_network_timeout)) != 0)
	{
		RETURN_BOOL(false);
	}

	RETURN_BOOL(true);
}

static void php_fdfs_get_file_info_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext, const bool bFileId)
{
	int result;
	int argc;
	char *group_name;
	char *remote_filename;
	zend_size_t group_nlen;
	zend_size_t filename_len;
	int param_count;
	FDFSFileInfo file_info;
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128];

	if (bFileId)
	{
		param_count = 1;
	}
	else
	{
		param_count = 2;
	}

    	argc = ZEND_NUM_ARGS();
	if (argc != param_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_upload_file parameters " \
			"count: %d != %d", __LINE__, argc, param_count);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (bFileId)
	{
		char *pSeperator;
		char *file_id;
		zend_size_t file_id_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", \
			&file_id, &file_id_len) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		snprintf(new_file_id, sizeof(new_file_id), "%s", file_id);
		pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR);
		if (pSeperator == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"file_id is invalid, file_id=%s", \
				__LINE__, file_id);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}

		*pSeperator = '\0';
		group_name = new_file_id;
		remote_filename =  pSeperator + 1;
	}
	else
	{
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", \
			&group_name, &group_nlen, &remote_filename, \
			&filename_len) == FAILURE)
		{
			logError("file: "__FILE__", line: %d, " \
				"zend_parse_parameters fail!", __LINE__);
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}
	}

	result = fdfs_get_file_info_ex(group_name, remote_filename, true, &file_info);
	pContext->err_no = result;
	if (result != 0)
	{
		RETURN_BOOL(false);
	}

	array_init(return_value);
	zend_add_assoc_long_ex(return_value, "source_id", \
		sizeof("source_id"), file_info.source_id);
	zend_add_assoc_long_ex(return_value, "create_timestamp", \
		sizeof("create_timestamp"), file_info.create_timestamp);
	zend_add_assoc_long_ex(return_value, "file_size", \
		sizeof("file_size"), (long)file_info.file_size);
	zend_add_assoc_stringl_ex(return_value, "source_ip_addr", \
		sizeof("source_ip_addr"), file_info.source_ip_addr, \
		strlen(file_info.source_ip_addr), 1);
	zend_add_assoc_long_ex(return_value, "crc32", \
		sizeof("crc32"), file_info.crc32);
}

/*
string fastdfs_gen_slave_filename(string master_filename, string prefix_name
		[, string file_ext_name])
return slave filename string for success, false for error
*/
static void php_fdfs_gen_slave_filename_impl(INTERNAL_FUNCTION_PARAMETERS, \
		FDFSPhpContext *pContext)
{
	int result;
	int argc;
	char *master_filename;
	zend_size_t master_filename_len;
	char *prefix_name;
	zend_size_t prefix_name_len;
	int filename_len;
	zval *ext_name_obj;
	char *file_ext_name;
	int file_ext_name_len;
	char filename[128];

    	argc = ZEND_NUM_ARGS();
	if (argc != 2 && argc != 3)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage_upload_file parameters " \
			"count: %d != 2 or 3", __LINE__, argc);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	ext_name_obj = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|z", \
		&master_filename, &master_filename_len, &prefix_name, \
		&prefix_name_len, &ext_name_obj) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	if (ext_name_obj == NULL)
	{
		file_ext_name = NULL;
		file_ext_name_len = 0;
	}
	else
	{
		if (ZEND_TYPE_OF(ext_name_obj) == IS_NULL)
		{
			file_ext_name = NULL;
			file_ext_name_len = 0;
		}
		else if (ZEND_TYPE_OF(ext_name_obj) == IS_STRING)
		{
			file_ext_name = Z_STRVAL_P(ext_name_obj);
			file_ext_name_len = Z_STRLEN_P(ext_name_obj);
		}
		else
		{
			logError("file: "__FILE__", line: %d, " \
				"file_ext_name is not a string, type=%d!", \
				__LINE__, ZEND_TYPE_OF(ext_name_obj));
			pContext->err_no = EINVAL;
			RETURN_BOOL(false);
		}
	}

	if (master_filename_len + prefix_name_len + file_ext_name_len + 1 \
		>= sizeof(filename))
	{
		logError("file: "__FILE__", line: %d, " \
			"filename length is too long!", __LINE__);
		pContext->err_no = EINVAL;
		RETURN_BOOL(false);
	}

	result = fdfs_gen_slave_filename(master_filename, \
			prefix_name, file_ext_name, filename, &filename_len);
	pContext->err_no = result;
	if (result != 0)
	{
		RETURN_BOOL(false);
	}

	ZEND_RETURN_STRINGL(filename, filename_len, 1);
}

/*
array fastdfs_tracker_get_connection()
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_get_connection)
{
	php_fdfs_tracker_get_connection_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context);
}

/*
boolean fastdfs_tracker_make_all_connections()
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_make_all_connections)
{
	php_fdfs_tracker_make_all_connections_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context);
}

/*
boolean fastdfs_tracker_close_all_connections()
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_close_all_connections)
{
	php_fdfs_tracker_close_all_connections_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context);
}

/*
array fastdfs_connect_server(string ip_addr, int port)
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_connect_server)
{
	php_fdfs_connect_server_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context);
}

/*
boolean fastdfs_disconnect_server(array serverInfo)
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_disconnect_server)
{
	php_fdfs_disconnect_server_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context);
}

/*
boolean fastdfs_active_test(array serverInfo)
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_active_test)
{
	php_fastdfs_active_test_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context);
}

/*
long fastdfs_get_last_error_no()
return last error no
*/
ZEND_FUNCTION(fastdfs_get_last_error_no)
{
	RETURN_LONG(php_context.err_no);
}

/*
string fastdfs_get_last_error_info()
return last error info
*/
ZEND_FUNCTION(fastdfs_get_last_error_info)
{
	char *error_info;

	error_info = STRERROR(php_context.err_no);
	ZEND_RETURN_STRINGL(error_info, strlen(error_info), 1);
}

/*
string fastdfs_client_version()
return client library version
*/
ZEND_FUNCTION(fastdfs_client_version)
{
	char szVersion[16];
	int len;

	len = sprintf(szVersion, "%d.%02d", \
		g_fdfs_version.major, g_fdfs_version.minor);

	ZEND_RETURN_STRINGL(szVersion, len, 1);
}

/*
array fastdfs_tracker_list_groups([string group_name, array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_list_groups)
{
	php_fdfs_tracker_list_groups_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
			&php_context);
}

/*
array fastdfs_tracker_query_storage_store([string group_name, 
		array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_query_storage_store)
{
	php_fdfs_tracker_query_storage_store_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context);
}

/*
array fastdfs_tracker_query_storage_store_list([string group_name, 
		array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_query_storage_store_list)
{
	php_fdfs_tracker_query_storage_store_list_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context);
}

/*
array fastdfs_tracker_query_storage_update(string group_name, 
		string remote_filename [, array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_query_storage_update)
{
	php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE, false);
}

/*
array fastdfs_tracker_query_storage_fetch(string group_name, 
		string remote_filename [, array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_query_storage_fetch)
{
	php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE, false);
}

/*
array fastdfs_tracker_query_storage_list(string group_name, 
		string remote_filename [, array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_query_storage_list)
{
	php_fdfs_tracker_query_storage_list_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, false);
}

/*
array fastdfs_tracker_query_storage_update1(string file_id, 
		[, array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_query_storage_update1)
{
	php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE, true);
}

/*
array fastdfs_tracker_query_storage_fetch1(string file_id 
		[, array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_query_storage_fetch1)
{
	php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE, true);
}

/*
array fastdfs_tracker_query_storage_list1(string file_id
		[, array tracker_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_query_storage_list1)
{
	php_fdfs_tracker_query_storage_list_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, true);
}

/*
boolean fastdfs_tracker_delete_storage(string group_name, string storage_ip)
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_tracker_delete_storage)
{
	php_fdfs_tracker_delete_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context);
}

/*
array fastdfs_storage_upload_by_filename(string local_filename, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_by_filename)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_FILE, false);
}

/*
string fastdfs_storage_upload_by_filename1(string local_filename, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_by_filename1)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_FILE, true);
}

/*
array fastdfs_storage_upload_by_filebuff(string file_buff, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_by_filebuff)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_BUFF, false);
}

/*
string fastdfs_storage_upload_by_filebuff1(string file_buff, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id  for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_by_filebuff1)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_BUFF, true);
}

/*
array fastdfs_storage_upload_by_callback(array callback_array, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_by_callback)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
string fastdfs_storage_upload_by_callback1(array callback_array, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id  for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_by_callback1)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_CALLBACK, true);
}

/*
boolean fastdfs_storage_append_by_filename(string local_filename, 
	string group_name, appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_append_by_filename)
{
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_FILE, false);
}

/*
boolean fastdfs_storage_append_by_filename1(string local_filename, 
	string appender_file_id
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_append_by_filename1)
{
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_FILE, true);
}

/*
boolean fastdfs_storage_append_by_filebuff(string file_buff, 
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_append_by_filebuff)
{
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_BUFF, false);
}

/*
boolean fastdfs_storage_append_by_filebuff1(string file_buff, 
	string appender_file_id
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_append_by_filebuff1)
{
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_BUFF, true);
}

/*
boolean fastdfs_storage_append_by_callback(array callback_array, 
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_append_by_callback)
{
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
boolean fastdfs_storage_append_by_callback1(array callback_array, 
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_append_by_callback1)
{
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_CALLBACK, true);
}

/*
boolean fastdfs_storage_modify_by_filename(string local_filename, 
	long file_offset, string group_name, appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_modify_by_filename)
{
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_FILE, false);
}

/*
boolean fastdfs_storage_modify_by_filename1(string local_filename, 
	long file_offset, string appender_file_id
        [, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_modify_by_filename1)
{
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_FILE, true);
}

/*
boolean fastdfs_storage_modify_by_filebuff(string file_buff, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_modify_by_filebuff)
{
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_BUFF, false);
}

/*
boolean fastdfs_storage_modify_by_filebuff1(string file_buff, 
	long file_offset, string appender_file_id
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_modify_by_filebuff1)
{
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_BUFF, true);
}

/*
boolean fastdfs_storage_modify_by_callback(array callback_array, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_modify_by_callback)
{
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
boolean fastdfs_storage_modify_by_callback1(array callback_array, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_modify_by_callback1)
{
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, FDFS_UPLOAD_BY_CALLBACK, true);
}

/*
array fastdfs_storage_upload_appender_by_filename(string local_filename, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_appender_by_filename)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_FILE, false);
}

/*
string fastdfs_storage_upload_appender_by_filename1(string local_filename, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_appender_by_filename1)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_FILE, true);
}

/*
array fastdfs_storage_upload_appender_by_filebuff(string file_buff, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_appender_by_filebuff)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_BUFF, false);
}

/*
string fastdfs_storage_upload_appender_by_filebuff1(string file_buff, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id  for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_appender_by_filebuff1)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_BUFF, true);
}

/*
array fastdfs_storage_upload_appender_by_callback(array callback_array, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_appender_by_callback)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
string fastdfs_storage_upload_appender_by_callback1(array callback_array, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_appender_by_callback1)
{
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_CALLBACK, true);
}

/*
string/array fastdfs_storage_upload_slave_by_filename(string local_filename, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
return string/array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_filename)
{
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		FDFS_UPLOAD_BY_FILE, false);
}

/*
string fastdfs_storage_upload_slave_by_filename1(string local_filename, 
	string master_file_id, string prefix_name [, string file_ext_name, 
	string meta_list, array tracker_server, array storage_server])
return file_id for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_filename1)
{
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		FDFS_UPLOAD_BY_FILE, true);
}

/*
array fastdfs_storage_upload_slave_by_filebuff(string file_buff, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_filebuff)
{
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		FDFS_UPLOAD_BY_BUFF, false);
}

/*
string fastdfs_storage_upload_slave_by_filebuff1(string file_buff, 
	string master_file_id, string prefix_name [, string file_ext_name, 
	string meta_list, array tracker_server, array storage_server])
return file_id  for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_filebuff1)
{
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		FDFS_UPLOAD_BY_BUFF, true);
}

/*
array fastdfs_storage_upload_slave_by_callback(array callback_array, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_callback)
{
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
string fastdfs_storage_upload_slave_by_callback1(array callback_array, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, array meta_list, 
	array tracker_server, array storage_server])
return file_id  for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_callback1)
{
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, \
		FDFS_UPLOAD_BY_CALLBACK, true);
}

/*
boolean fastdfs_storage_delete_file(string group_name, string remote_filename 
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_delete_file)
{
	php_fdfs_storage_delete_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, false);
}

/*
boolean fastdfs_storage_delete_file1(string file_id
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_delete_file1)
{
	php_fdfs_storage_delete_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, true);
}

/*
boolean fastdfs_storage_truncate_file(string group_name, 
	string appender_filename [, long truncated_file_size = 0, 
	array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_truncate_file)
{
	php_fdfs_storage_truncate_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, false);
}

/*
boolean fastdfs_storage_truncate_file1(string appender_file_id
	[, long truncated_file_size = 0, array tracker_server, 
	array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_truncate_file1)
{
	php_fdfs_storage_truncate_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&php_context, true);
}

/*
string fastdfs_storage_download_file_to_buff(string group_name, 
	string remote_filename [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
return file content for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_download_file_to_buff)
{
	php_fdfs_storage_download_file_to_buff_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, false);
}

/*
string fastdfs_storage_download_file_to_buff1(string file_id
        [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
return file content for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_download_file_to_buff1)
{
	php_fdfs_storage_download_file_to_buff_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, true);
}

/*
boolean fastdfs_storage_download_file_to_callback(string group_name,
	string remote_filename, array download_callback [, long file_offset, 
	long download_bytes, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_download_file_to_callback)
{
	php_fdfs_storage_download_file_to_callback_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, false);
}

/*
boolean fastdfs_storage_download_file_to_callback1(string file_id,
	array download_callback [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_download_file_to_callback1)
{
	php_fdfs_storage_download_file_to_callback_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, true);
}

/*
boolean fastdfs_storage_download_file_to_file(string group_name, 
	string remote_filename, string local_filename [, long file_offset, 
	long download_bytes, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_download_file_to_file)
{
	php_fdfs_storage_download_file_to_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, false);
}

/*
boolean fastdfs_storage_download_file_to_file1(string file_id, 
	string local_filename [, long file_offset, long download_bytes, 
	array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_download_file_to_file1)
{
	php_fdfs_storage_download_file_to_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, true);
}

/*
boolean fastdfs_storage_set_metadata(string group_name, string remote_filename,
	array meta_list [, string op_type, array tracker_server, 
	array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_set_metadata)
{
	php_fdfs_storage_set_metadata_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, false);
}

/*
boolean fastdfs_storage_set_metadata1(string file_id, array meta_list
	[, string op_type, array tracker_server, array storage_server])
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_set_metadata1)
{
	php_fdfs_storage_set_metadata_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, true);
}

/*
array fastdfs_storage_get_metadata(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_get_metadata)
{
	php_fdfs_storage_get_metadata_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, false);
}

/*
array fastdfs_storage_get_metadata1(string file_id
	[, array tracker_server, array storage_server])
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_storage_get_metadata1)
{
	php_fdfs_storage_get_metadata_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, true);
}

/*
boolean fastdfs_storage_file_exist(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
return true for exist, false for not exist
*/
ZEND_FUNCTION(fastdfs_storage_file_exist)
{
	php_fdfs_storage_file_exist_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, false);
}

/*
boolean fastdfs_storage_file_exist1(string file_id
	[, array tracker_server, array storage_server])
return true for exist, false for not exist
*/
ZEND_FUNCTION(fastdfs_storage_file_exist1)
{
	php_fdfs_storage_file_exist_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, true);
}

/*
string fastdfs_http_gen_token(string file_id, int timestamp)
return token string for success, false for error
*/
ZEND_FUNCTION(fastdfs_http_gen_token)
{
	php_fdfs_http_gen_token_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context);
}

/*
array fastdfs_get_file_info(string group_name, string remote_filename)
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_get_file_info)
{
	php_fdfs_get_file_info_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, false);
}

/*
array fastdfs_get_file_info1(string file_id)
return array for success, false for error
*/
ZEND_FUNCTION(fastdfs_get_file_info1)
{
	php_fdfs_get_file_info_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context, true);
}

/*
bool fastdfs_send_data(int sock, string buff)
return true for success, false for error
*/
ZEND_FUNCTION(fastdfs_send_data)
{
	php_fdfs_send_data_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context);
}

/*
string fastdfs_gen_slave_filename(string master_filename, string prefix_name
		[, string file_ext_name])
return slave filename string for success, false for error
*/
ZEND_FUNCTION(fastdfs_gen_slave_filename)
{
	php_fdfs_gen_slave_filename_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &php_context);
}

static void php_fdfs_close(php_fdfs_t *i_obj TSRMLS_DC)
{
	if (i_obj->context.pTrackerGroup == NULL)
	{
		return;
	}

	if (i_obj->context.pTrackerGroup != i_obj->pConfigInfo->pTrackerGroup)
	{
		tracker_close_all_connections_ex(i_obj->context.pTrackerGroup);
	}
}

static void php_fdfs_destroy(php_fdfs_t *i_obj TSRMLS_DC)
{
	php_fdfs_close(i_obj TSRMLS_CC);
	if (i_obj->context.pTrackerGroup != NULL && i_obj->context.pTrackerGroup != \
		i_obj->pConfigInfo->pTrackerGroup)
	{
		fdfs_client_destroy_ex(i_obj->context.pTrackerGroup);
		efree(i_obj->context.pTrackerGroup);
		i_obj->context.pTrackerGroup = NULL;
	}
}

/* FastDFS::__construct([int config_index = 0, bool bMultiThread = false])
   Creates a FastDFS object */
static PHP_METHOD(FastDFS, __construct)
{
	long config_index;
	bool bMultiThread;
	zval *object = getThis();
	php_fdfs_t *i_obj = NULL;

	config_index = 0;
	bMultiThread = false;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lb", \
			&config_index, &bMultiThread) == FAILURE)
	{
		logError("file: "__FILE__", line: %d, " \
			"zend_parse_parameters fail!", __LINE__);
		ZVAL_NULL(object);
		return;
	}

	if (config_index < 0 || config_index >= config_count)
	{
		logError("file: "__FILE__", line: %d, " \
			"invalid config_index: %ld < 0 || >= %d", \
			__LINE__, config_index, config_count);
		ZVAL_NULL(object);
		return;
	}

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	i_obj->pConfigInfo = config_list + config_index;
	i_obj->context.err_no = 0;
	if (bMultiThread)
	{
		i_obj->context.pTrackerGroup = (TrackerServerGroup *)emalloc( \
					sizeof(TrackerServerGroup));
		if (i_obj->context.pTrackerGroup == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail!", __LINE__, \
				(int)sizeof(TrackerServerGroup));
			ZVAL_NULL(object);
			return;
		}

		if (fdfs_copy_tracker_group(i_obj->context.pTrackerGroup, \
			i_obj->pConfigInfo->pTrackerGroup) != 0)
		{
			ZVAL_NULL(object);
			return;
		}
	}
	else
	{
		i_obj->context.pTrackerGroup = i_obj->pConfigInfo->pTrackerGroup;
	}
}

/*
array FastDFS::tracker_get_connection()
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_get_connection)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_get_connection_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
			&(i_obj->context));
}

/*
boolean  FastDFS::tracker_make_all_connections()
return true for success, false for error
*/
PHP_METHOD(FastDFS, tracker_make_all_connections)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_make_all_connections_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context));
}

/*
boolean  FastDFS::tracker_close_all_connections()
return true for success, false for error
*/
PHP_METHOD(FastDFS, tracker_close_all_connections)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_close_all_connections_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context));
}

/*
array FastDFS::connect_server(string ip_addr, int port)
return array for success, false for error
*/
PHP_METHOD(FastDFS, connect_server)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_connect_server_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context));
}

/*
boolean FastDFS::disconnect_server(array serverInfo)
return true for success, false for error
*/
PHP_METHOD(FastDFS, disconnect_server)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_disconnect_server_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context));
}

/*
boolean FastDFS::active_test(array serverInfo)
return true for success, false for error
*/
PHP_METHOD(FastDFS, active_test)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fastdfs_active_test_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context));
}

/*
array FastDFS::tracker_list_groups([string group_name, array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_list_groups)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_list_groups_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
			&(i_obj->context));
}

/*
array FastDFS::tracker_query_storage_store([string group_name, 
		array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_query_storage_store)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_query_storage_store_impl( \
			INTERNAL_FUNCTION_PARAM_PASSTHRU, \
			&(i_obj->context));
}

/*
array FastDFS::tracker_query_storage_store_list([string group_name, 
		array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_query_storage_store_list)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_query_storage_store_list_impl( \
			INTERNAL_FUNCTION_PARAM_PASSTHRU, \
			&(i_obj->context));
}

/*
array FastDFS::tracker_query_storage_update(string group_name, 
		string remote_filename [, array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_query_storage_update)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE, false);
}

/*
array FastDFS::tracker_query_storage_fetch(string group_name, 
		string remote_filename [, array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_query_storage_fetch)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE, false);
}

/*
array FastDFS::tracker_query_storage_list(string group_name, 
		string remote_filename [, array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_query_storage_list)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_query_storage_list_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), false);
}

/*
boolean FastDFS::tracker_delete_storage(string group_name, string storage_ip)
return true for success, false for error
*/
PHP_METHOD(FastDFS, tracker_delete_storage)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_delete_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context));
}

/*
array FastDFS::tracker_query_storage_update1(string file_id 
		[, array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_query_storage_update1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE, true);
}

/*
array FastDFS::tracker_query_storage_fetch1(string file_id 
		[, array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_query_storage_fetch1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_do_query_storage_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE, true);
}

/*
array FastDFS::tracker_query_storage_list1(string file_id 
		[, array tracker_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, tracker_query_storage_list1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_tracker_query_storage_list_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), true);
}

/*
array FastDFS::storage_upload_by_filename(string local_filename, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_by_filename)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_FILE, false);
}

/*
string FastDFS::storage_upload_by_filename1(string local_filename, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_by_filename1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_FILE, true);
}

/*
array FastDFS::storage_upload_by_filebuff(string file_buff, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_by_filebuff)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_BUFF, false);
}

/*
string FastDFS::storage_upload_by_filebuff1(string file_buff, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_by_filebuff1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_BUFF, true);
}

/*
array FastDFS::storage_upload_by_callback(array callback_array, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_by_callback)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
string FastDFS::storage_upload_by_callback1(array callback_array, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id  for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_by_callback1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_CALLBACK, true);
}

/*
boolean FastDFS::storage_append_by_filename(string local_filename, 
	string group_name, appender_filename
	[, array tracker_server, array storage_server])
return string/array for success, false for error
*/
PHP_METHOD(FastDFS, storage_append_by_filename)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_FILE, false);
}

/*
string FastDFS::storage_upload_by_filename1(string local_filename, 
	string appender_file_id
	[, array tracker_server, array storage_server])
return file_id for success, false for error
*/
PHP_METHOD(FastDFS, storage_append_by_filename1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_FILE, true);
}

/*
array FastDFS::storage_append_by_filebuff(string file_buff, 
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_append_by_filebuff)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_BUFF, false);
}

/*
string FastDFS::storage_append_by_filebuff1(string file_buff, 
	string appender_file_id
	[, array tracker_server, array storage_server])
return file_id  for success, false for error
*/
PHP_METHOD(FastDFS, storage_append_by_filebuff1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_BUFF, true);
}

/*
array FastDFS::storage_append_by_callback(array callback_array, 
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_append_by_callback)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
string FastDFS::storage_append_by_callback1(array callback_array,
	string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return file_id  for success, false for error
*/
PHP_METHOD(FastDFS, storage_append_by_callback1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_append_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_CALLBACK, true);
}


/*
boolean FastDFS::storage_modify_by_filename(string local_filename, 
	long file_offset, string group_name, appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_modify_by_filename)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_FILE, false);
}

/*
boolean FastDFS::storage_modify_by_filename1(string local_filename, 
	long file_offset, string appender_file_id
        [, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_modify_by_filename1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_FILE, true);
}

/*
boolean FastDFS::storage_modify_by_filebuff(string file_buff, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_modify_by_filebuff)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_BUFF, false);
}

/*
boolean FastDFS::storage_modify_by_filebuff1(string file_buff, 
	long file_offset, string appender_file_id
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_modify_by_filebuff1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_BUFF, true);
}

/*
boolean FastDFS::storage_modify_by_callback(array callback_array, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_modify_by_callback)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
boolean FastDFS::storage_modify_by_callback1(array callback_array, 
	long file_offset, string group_name, string appender_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_modify_by_callback1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_modify_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), FDFS_UPLOAD_BY_CALLBACK, true);
}

/*
array FastDFS::storage_upload_appender_by_filename(string local_filename, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_appender_by_filename)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_FILE, false);
}

/*
string FastDFS::storage_upload_appender_by_filename1(string local_filename, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_appender_by_filename1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_FILE, true);
}

/*
array FastDFS::storage_upload_appender_by_filebuff(string file_buff, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_appender_by_filebuff)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_BUFF, false);
}

/*
string FastDFS::storage_upload_appender_by_filebuff1(string file_buff, 
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_appender_by_filebuff1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_BUFF, true);
}

/*
array FastDFS::storage_upload_appender_by_callback(array callback_array,
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_appender_by_callback)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
string FastDFS::storage_upload_appender_by_callback1(array callback_array,
	[string file_ext_name, string meta_list, string group_name, 
	array tracker_server, array storage_server])
return file_id  for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_appender_by_callback1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_CALLBACK, true);
}


/*
array FastDFS::storage_upload_slave_by_filename(string local_filename, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, string meta_list, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_slave_by_filename)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		FDFS_UPLOAD_BY_FILE, false);
}

/*
string FastDFS::storage_upload_slave_by_filename1(string local_filename, 
	string master_file_id, string prefix_name 
	[, string file_ext_name, string meta_list, 
	array tracker_server, array storage_server])
return file_id for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_slave_by_filename1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		FDFS_UPLOAD_BY_FILE, true);
}

/*
array FastDFS::storage_upload_slave_by_filebuff(string file_buff, 
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, string meta_list, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_slave_by_filebuff)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		FDFS_UPLOAD_BY_BUFF, false);
}

/*
string FastDFS::storage_upload_slave_by_filebuff1(string file_buff, 
	string master_file_id, string prefix_name [, string file_ext_name, 
	string meta_list, array tracker_server, array storage_server])
return file_id  for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_slave_by_filebuff1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		FDFS_UPLOAD_BY_BUFF, true);
}

/*
array FastDFS::storage_upload_slave_by_callback(array callback_array,
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, string meta_list, 
	array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_slave_by_callback)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		FDFS_UPLOAD_BY_CALLBACK, false);
}

/*
string FastDFS::storage_upload_slave_by_callback1(array callback_array,
	string group_name, string master_filename, string prefix_name 
	[, string file_ext_name, string meta_list, 
	array tracker_server, array storage_server])
return file_id  for success, false for error
*/
PHP_METHOD(FastDFS, storage_upload_slave_by_callback1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_upload_slave_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), \
		FDFS_UPLOAD_BY_CALLBACK, true);
}

/*
boolean FastDFS::storage_delete_file(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_delete_file)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_delete_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), false);
}

/*
boolean FastDFS::storage_delete_file1(string file_id
	[, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_delete_file1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_delete_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), true);
}

/*
boolean FastDFS::storage_truncate_file(string group_name, 
	string remote_filename [, long truncated_file_size = 0, 
	array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_truncate_file)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_truncate_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), false);
}

/*
boolean FastDFS::storage_truncate_file1(string file_id
	[, long truncated_file_size = 0, array tracker_server, 
	array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_truncate_file1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_truncate_file_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, \
		&(i_obj->context), true);
}

/*
string FastDFS::storage_download_file_to_buff(string group_name, 
	string remote_filename [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
return file content for success, false for error
*/
PHP_METHOD(FastDFS, storage_download_file_to_buff)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_download_file_to_buff_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), false);
}

/*
string FastDFS::storage_download_file_to_buff1(string file_id
        [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
return file content for success, false for error
*/
PHP_METHOD(FastDFS, storage_download_file_to_buff1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_download_file_to_buff_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), true);
}

/*
boolean FastDFS::storage_download_file_to_callback(string group_name,
	string remote_filename, array download_callback [, long file_offset, 
	long download_bytes, array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_download_file_to_callback)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_download_file_to_callback_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), false);
}

/*
boolean FastDFS::storage_download_file_to_callback1(string file_id, 
	array download_callback [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_download_file_to_callback1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_download_file_to_callback_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), true);
}

/*
boolean FastDFS::storage_download_file_to_file(string group_name, 
	string remote_filename, string local_filename 
	[, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_download_file_to_file)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_download_file_to_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), false);
}

/*
boolean FastDFS::storage_download_file_to_file1(string file_id,
        string local_filename, [, long file_offset, long download_bytes,
	array tracker_server, array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_download_file_to_file1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_download_file_to_file_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), true);
}

/*
boolean FastDFS::storage_set_metadata(string group_name, string remote_filename,
	array meta_list [, string op_type, array tracker_server, 
	array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_set_metadata)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_set_metadata_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), false);
}

/*
boolean FastDFS::storage_set_metadata1(string file_id,
	array meta_list [, string op_type, array tracker_server, 
	array storage_server])
return true for success, false for error
*/
PHP_METHOD(FastDFS, storage_set_metadata1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_set_metadata_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), true);
}

/*
array FastDFS::storage_get_metadata(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_get_metadata)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_get_metadata_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), false);
}

/*
array FastDFS::storage_get_metadata1(string file_id
	[, array tracker_server, array storage_server])
return array for success, false for error
*/
PHP_METHOD(FastDFS, storage_get_metadata1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_get_metadata_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), true);
}

/*
boolean FastDFS::storage_file_exist(string group_name, string remote_filename
	[, array tracker_server, array storage_server])
return true for exist, false for not exist
*/
PHP_METHOD(FastDFS, storage_file_exist)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_file_exist_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), false);
}

/*
boolean FastDFS::storage_file_exist1(string file_id
	[, array tracker_server, array storage_server])
return true for exist, false for not exist
*/
PHP_METHOD(FastDFS, storage_file_exist1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_storage_file_exist_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), true);
}

/*
long FastDFS::get_last_error_no()
return last error no
*/
PHP_METHOD(FastDFS, get_last_error_no)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	RETURN_LONG(i_obj->context.err_no);
}

/*
string FastDFS::get_last_error_info()
return last error info
*/
PHP_METHOD(FastDFS, get_last_error_info)
{
	char *error_info;
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	error_info = STRERROR(i_obj->context.err_no);
	ZEND_RETURN_STRINGL(error_info, strlen(error_info), 1);
}

/*
string FastDFS::http_gen_token(string file_id, int timestamp)
return token string for success, false for error
*/
PHP_METHOD(FastDFS, http_gen_token)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_http_gen_token_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context));
}
                
/*
array FastDFS::get_file_info(string group_name, string remote_filename)
return array for success, false for error
*/
PHP_METHOD(FastDFS, get_file_info)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_get_file_info_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), false);
}

/*
array FastDFS::get_file_info1(string file_id)
return array for success, false for error
*/
PHP_METHOD(FastDFS, get_file_info1)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_get_file_info_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context), true);
}

/*
bool FastDFS::send_data(int sock, string buff)
return true for success, false for error
*/
PHP_METHOD(FastDFS, send_data)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_send_data_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context));
}

/*
string FastDFS::gen_slave_filename(string master_filename, string prefix_name
		[, string file_ext_name])
return slave filename string for success, false for error
*/
PHP_METHOD(FastDFS, gen_slave_filename)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_gen_slave_filename_impl( \
		INTERNAL_FUNCTION_PARAM_PASSTHRU, &(i_obj->context));
}

/*
void FastDFS::close()
*/
PHP_METHOD(FastDFS, close)
{
	zval *object = getThis();
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *) fdfs_get_object(object);
	php_fdfs_close(i_obj TSRMLS_CC);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo___construct, 0, 0, 0)
ZEND_ARG_INFO(0, config_index)
ZEND_ARG_INFO(0, bMultiThread)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_get_connection, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_make_all_connections, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_close_all_connections, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_connect_server, 0, 0, 2)
ZEND_ARG_INFO(0, ip_addr)
ZEND_ARG_INFO(0, port)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_disconnect_server, 0, 0, 1)
ZEND_ARG_INFO(0, server_info)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_active_test, 0, 0, 1)
ZEND_ARG_INFO(0, server_info)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_list_groups, 0, 0, 0)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_query_storage_store, 0, 0, 0)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_query_storage_store_list, 0, 0, 0)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_query_storage_update, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_query_storage_fetch, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_query_storage_list, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_query_storage_update1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_query_storage_fetch1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_query_storage_list1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tracker_delete_storage, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, storage_ip)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_by_filename, 0, 0, 1)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_by_filename1, 0, 0, 1)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_by_filebuff, 0, 0, 1)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_by_filebuff1, 0, 0, 1)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_by_callback, 0, 0, 1)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_by_callback1, 0, 0, 1)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_append_by_filename, 0, 0, 3)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, appender_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_append_by_filename1, 0, 0, 2)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, appender_file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_append_by_filebuff, 0, 0, 3)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, appender_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_append_by_filebuff1, 0, 0, 2)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, appender_file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_append_by_callback, 0, 0, 3)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, appender_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_append_by_callback1, 0, 0, 2)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, appender_file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_modify_by_filename, 0, 0, 3)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, appender_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_modify_by_filename1, 0, 0, 2)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, appender_file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_modify_by_filebuff, 0, 0, 3)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, appender_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_modify_by_filebuff1, 0, 0, 2)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, appender_file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_modify_by_callback, 0, 0, 3)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, appender_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_modify_by_callback1, 0, 0, 2)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, appender_file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_appender_by_filename, 0, 0, 1)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_appender_by_filename1, 0, 0, 1)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_appender_by_filebuff, 0, 0, 1)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_appender_by_filebuff1, 0, 0, 1)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_appender_by_callback, 0, 0, 1)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_appender_by_callback1, 0, 0, 1)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_slave_by_filename, 0, 0, 4)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, master_filename)
ZEND_ARG_INFO(0, prefix_name)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_slave_by_filename1, 0, 0, 3)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, master_file_id)
ZEND_ARG_INFO(0, prefix_name)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_slave_by_filebuff, 0, 0, 4)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, master_filename)
ZEND_ARG_INFO(0, prefix_name)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_slave_by_filebuff1, 0, 0, 3)
ZEND_ARG_INFO(0, file_buff)
ZEND_ARG_INFO(0, master_file_id)
ZEND_ARG_INFO(0, prefix_name)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_slave_by_callback, 0, 0, 4)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, master_filename)
ZEND_ARG_INFO(0, prefix_name)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_upload_slave_by_callback1, 0, 0, 3)
ZEND_ARG_INFO(0, callback_array)
ZEND_ARG_INFO(0, master_file_id)
ZEND_ARG_INFO(0, prefix_name)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_delete_file, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_delete_file1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_truncate_file, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, truncated_file_size)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_truncate_file1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, truncated_file_size)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_download_file_to_buff, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, file_offset)
ZEND_ARG_INFO(0, download_bytes)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_download_file_to_buff1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, file_offset)
ZEND_ARG_INFO(0, download_bytes)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_download_file_to_callback, 0, 0, 3)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, download_callback)
ZEND_ARG_INFO(0, file_offset)
ZEND_ARG_INFO(0, download_bytes)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_download_file_to_callback1, 0, 0, 2)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, download_callback)
ZEND_ARG_INFO(0, file_offset)
ZEND_ARG_INFO(0, download_bytes)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_download_file_to_file, 0, 0, 3)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, file_offset)
ZEND_ARG_INFO(0, download_bytes)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_download_file_to_file1, 0, 0, 2)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, local_filename)
ZEND_ARG_INFO(0, file_offset)
ZEND_ARG_INFO(0, download_bytes)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_set_metadata, 0, 0, 3)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, op_type)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_set_metadata1, 0, 0, 2)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, meta_list)
ZEND_ARG_INFO(0, op_type)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_get_metadata, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_get_metadata1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_file_exist, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_storage_file_exist1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, tracker_server)
ZEND_ARG_INFO(0, storage_server)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_get_last_error_no, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_get_last_error_info, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_http_gen_token, 0, 0, 2)
ZEND_ARG_INFO(0, file_id)
ZEND_ARG_INFO(0, timestamp)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_get_file_info, 0, 0, 2)
ZEND_ARG_INFO(0, group_name)
ZEND_ARG_INFO(0, remote_filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_get_file_info1, 0, 0, 1)
ZEND_ARG_INFO(0, file_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_send_data, 0, 0, 2)
ZEND_ARG_INFO(0, sock)
ZEND_ARG_INFO(0, buff)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gen_slave_filename, 0, 0, 2)
ZEND_ARG_INFO(0, master_filename)
ZEND_ARG_INFO(0, prefix_name)
ZEND_ARG_INFO(0, file_ext_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_close, 0, 0, 0)
ZEND_END_ARG_INFO()

/* {{{ fdfs_class_methods */
#define FDFS_ME(name, args) PHP_ME(FastDFS, name, args, ZEND_ACC_PUBLIC)
static zend_function_entry fdfs_class_methods[] = {
    FDFS_ME(__construct,        arginfo___construct)
    FDFS_ME(tracker_get_connection,   arginfo_tracker_get_connection)
    FDFS_ME(tracker_make_all_connections, arginfo_tracker_make_all_connections)
    FDFS_ME(tracker_close_all_connections,arginfo_tracker_close_all_connections)
    FDFS_ME(active_test,   arginfo_active_test)
    FDFS_ME(connect_server,   arginfo_connect_server)
    FDFS_ME(disconnect_server,   arginfo_disconnect_server)
    FDFS_ME(tracker_list_groups,   arginfo_tracker_list_groups)
    FDFS_ME(tracker_query_storage_store,  arginfo_tracker_query_storage_store)
    FDFS_ME(tracker_query_storage_store_list,  arginfo_tracker_query_storage_store_list)
    FDFS_ME(tracker_query_storage_update, arginfo_tracker_query_storage_update)
    FDFS_ME(tracker_query_storage_fetch,  arginfo_tracker_query_storage_fetch)
    FDFS_ME(tracker_query_storage_list,   arginfo_tracker_query_storage_list)
    FDFS_ME(tracker_query_storage_update1,arginfo_tracker_query_storage_update1)
    FDFS_ME(tracker_query_storage_fetch1, arginfo_tracker_query_storage_fetch1)
    FDFS_ME(tracker_query_storage_list1,  arginfo_tracker_query_storage_list1)
    FDFS_ME(tracker_delete_storage, arginfo_tracker_delete_storage)
    FDFS_ME(storage_upload_by_filename,  arginfo_storage_upload_by_filename)
    FDFS_ME(storage_upload_by_filename1, arginfo_storage_upload_by_filename1)
    FDFS_ME(storage_upload_by_filebuff,  arginfo_storage_upload_by_filebuff)
    FDFS_ME(storage_upload_by_filebuff1, arginfo_storage_upload_by_filebuff1)
    FDFS_ME(storage_upload_by_callback,  arginfo_storage_upload_by_callback)
    FDFS_ME(storage_upload_by_callback1, arginfo_storage_upload_by_callback1)
    FDFS_ME(storage_append_by_filename,  arginfo_storage_append_by_filename)
    FDFS_ME(storage_append_by_filename1, arginfo_storage_append_by_filename1)
    FDFS_ME(storage_append_by_filebuff,  arginfo_storage_append_by_filebuff)
    FDFS_ME(storage_append_by_filebuff1, arginfo_storage_append_by_filebuff1)
    FDFS_ME(storage_append_by_callback,  arginfo_storage_append_by_callback)
    FDFS_ME(storage_append_by_callback1,  arginfo_storage_append_by_callback1)
    FDFS_ME(storage_modify_by_filename,  arginfo_storage_modify_by_filename)
    FDFS_ME(storage_modify_by_filename1, arginfo_storage_modify_by_filename1)
    FDFS_ME(storage_modify_by_filebuff,  arginfo_storage_modify_by_filebuff)
    FDFS_ME(storage_modify_by_filebuff1, arginfo_storage_modify_by_filebuff1)
    FDFS_ME(storage_modify_by_callback,  arginfo_storage_modify_by_callback)
    FDFS_ME(storage_modify_by_callback1,  arginfo_storage_modify_by_callback1)
    FDFS_ME(storage_upload_appender_by_filename,  arginfo_storage_upload_appender_by_filename)
    FDFS_ME(storage_upload_appender_by_filename1, arginfo_storage_upload_appender_by_filename1)
    FDFS_ME(storage_upload_appender_by_filebuff,  arginfo_storage_upload_appender_by_filebuff)
    FDFS_ME(storage_upload_appender_by_filebuff1, arginfo_storage_upload_appender_by_filebuff1)
    FDFS_ME(storage_upload_appender_by_callback, arginfo_storage_upload_appender_by_callback)
    FDFS_ME(storage_upload_appender_by_callback1, arginfo_storage_upload_appender_by_callback1)
    FDFS_ME(storage_upload_slave_by_filename,  arginfo_storage_upload_slave_by_filename)
    FDFS_ME(storage_upload_slave_by_filename1, arginfo_storage_upload_slave_by_filename1)
    FDFS_ME(storage_upload_slave_by_filebuff,  arginfo_storage_upload_slave_by_filebuff)
    FDFS_ME(storage_upload_slave_by_filebuff1, arginfo_storage_upload_slave_by_filebuff1)
    FDFS_ME(storage_upload_slave_by_callback, arginfo_storage_upload_slave_by_callback)
    FDFS_ME(storage_upload_slave_by_callback1, arginfo_storage_upload_slave_by_callback1)
    FDFS_ME(storage_delete_file,  arginfo_storage_delete_file)
    FDFS_ME(storage_delete_file1, arginfo_storage_delete_file1)
    FDFS_ME(storage_truncate_file,  arginfo_storage_truncate_file)
    FDFS_ME(storage_truncate_file1, arginfo_storage_truncate_file1)
    FDFS_ME(storage_download_file_to_buff, arginfo_storage_download_file_to_buff)
    FDFS_ME(storage_download_file_to_buff1,arginfo_storage_download_file_to_buff1)
    FDFS_ME(storage_download_file_to_file, arginfo_storage_download_file_to_file)
    FDFS_ME(storage_download_file_to_file1,arginfo_storage_download_file_to_file1)
    FDFS_ME(storage_download_file_to_callback, arginfo_storage_download_file_to_callback)
    FDFS_ME(storage_download_file_to_callback1, arginfo_storage_download_file_to_callback1)
    FDFS_ME(storage_set_metadata,  arginfo_storage_set_metadata)
    FDFS_ME(storage_set_metadata1, arginfo_storage_set_metadata1)
    FDFS_ME(storage_get_metadata,  arginfo_storage_get_metadata)
    FDFS_ME(storage_get_metadata1, arginfo_storage_get_metadata1)
    FDFS_ME(storage_file_exist,    arginfo_storage_file_exist)
    FDFS_ME(storage_file_exist1,   arginfo_storage_file_exist1)
    FDFS_ME(get_last_error_no,     arginfo_get_last_error_no)
    FDFS_ME(get_last_error_info,   arginfo_get_last_error_info)
    FDFS_ME(http_gen_token,        arginfo_http_gen_token)
    FDFS_ME(get_file_info,         arginfo_get_file_info)
    FDFS_ME(get_file_info1,        arginfo_get_file_info1)
    FDFS_ME(send_data,             arginfo_send_data)
    FDFS_ME(gen_slave_filename,    arginfo_gen_slave_filename)
    FDFS_ME(close,                 arginfo_close)
    { NULL, NULL, NULL }
};
#undef FDFS_ME
/* }}} */

#if PHP_MAJOR_VERSION < 7
static void php_fdfs_free_storage(void *object TSRMLS_DC)
{
    php_fdfs_t *i_obj = (php_fdfs_t *)object;
	zend_object_std_dtor(&i_obj->zo TSRMLS_CC);
	php_fdfs_destroy(i_obj TSRMLS_CC);
	efree(i_obj);
}
#else
static void php_fdfs_free_storage(zend_object *object)
{
    php_fdfs_t *i_obj = (php_fdfs_t *)((char*)(object) -
            XtOffsetOf(php_fdfs_t , zo));
	zend_object_std_dtor(&i_obj->zo TSRMLS_CC);
	php_fdfs_destroy(i_obj TSRMLS_CC);
}
#endif

#if PHP_MAJOR_VERSION < 7
zend_object_value php_fdfs_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value retval;
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *)ecalloc(1, sizeof(php_fdfs_t));

	zend_object_std_init(&i_obj->zo, ce TSRMLS_CC);
	retval.handle = zend_objects_store_put(i_obj, \
		(zend_objects_store_dtor_t)zend_objects_destroy_object, \
        php_fdfs_free_storage, NULL TSRMLS_CC);
	retval.handlers = zend_get_std_object_handlers();

	return retval;
}

#else

zend_object* php_fdfs_new(zend_class_entry *ce)
{
	php_fdfs_t *i_obj;

	i_obj = (php_fdfs_t *)ecalloc(1, sizeof(php_fdfs_t) + zend_object_properties_size(ce));

	zend_object_std_init(&i_obj->zo, ce TSRMLS_CC);
    object_properties_init(&i_obj->zo, ce);
    i_obj->zo.handlers = &fdfs_object_handlers;
	return &i_obj->zo;
}

#endif

PHP_FASTDFS_API zend_class_entry *php_fdfs_get_ce(void)
{
	return fdfs_ce;
}

PHP_FASTDFS_API zend_class_entry *php_fdfs_get_exception(void)
{
	return fdfs_exception_ce;
}

PHP_FASTDFS_API zend_class_entry *php_fdfs_get_exception_base(int root TSRMLS_DC)
{
#if HAVE_SPL
	if (!root)
	{
		if (!spl_ce_RuntimeException)
		{
			zend_class_entry *pce;
			zval *value;

			if (zend_hash_find_wrapper(CG(class_table), "runtimeexception",
			   sizeof("RuntimeException"), &value) == SUCCESS)
			{
				pce = Z_CE_P(value);
				spl_ce_RuntimeException = pce;
				return pce;
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			return spl_ce_RuntimeException;
		}
	}
#endif
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
	return zend_exception_get_default();
#else
	return zend_exception_get_default(TSRMLS_C);
#endif
}


static int load_config_files()
{
	#define ITEM_NAME_CONF_COUNT "fastdfs_client.tracker_group_count"
	#define ITEM_NAME_CONF_FILE  "fastdfs_client.tracker_group"
	#define ITEM_NAME_BASE_PATH  	 "fastdfs_client.base_path"
	#define ITEM_NAME_CONNECT_TIMEOUT "fastdfs_client.connect_timeout"
	#define ITEM_NAME_NETWORK_TIMEOUT "fastdfs_client.network_timeout"
	#define ITEM_NAME_LOG_LEVEL      "fastdfs_client.log_level"
	#define ITEM_NAME_LOG_FILENAME   "fastdfs_client.log_filename"
	#define ITEM_NAME_ANTI_STEAL_SECRET_KEY "fastdfs_client.http.anti_steal_secret_key"
	#define ITEM_NAME_USE_CONN_POOL  "fastdfs_client.use_connection_pool"
	#define ITEM_NAME_CONN_POOL_MAX_IDLE_TIME "fastdfs_client.connection_pool_max_idle_time"

	zval zarr[16];
	zval *pz;
	zval *conf_c;
	zval *base_path;
	zval *connect_timeout;
	zval *network_timeout;
	zval *log_level;
	zval *anti_steal_secret_key;
	zval *log_filename;
	zval *conf_filename;
	zval *use_conn_pool;
	zval *conn_pool_max_idle_time;
	char *pAntiStealSecretKey;
	char szItemName[sizeof(ITEM_NAME_CONF_FILE) + 10];
	int nItemLen;
	FDFSConfigInfo *pConfigInfo;
	FDFSConfigInfo *pConfigEnd;
	int result;

	pz = zarr;
	conf_c = pz++;
	base_path = pz++;
	connect_timeout = pz++;
	network_timeout = pz++;
	log_level = pz++;
	anti_steal_secret_key = pz++;
	log_filename = pz++;
	conf_filename = pz++;
	use_conn_pool = pz++;
	conn_pool_max_idle_time = pz++;

	if (zend_get_configuration_directive_wrapper(ITEM_NAME_CONF_COUNT, 
		sizeof(ITEM_NAME_CONF_COUNT), &conf_c) == SUCCESS)
	{
		config_count = atoi(Z_STRVAL_P(conf_c));
		if (config_count <= 0)
		{
			fprintf(stderr, "file: "__FILE__", line: %d, " \
				"fastdfs_client.ini, config_count: %d <= 0!\n",\
				__LINE__, config_count);
			return EINVAL;
		}
	}
	else
	{
		 config_count = 1;
	}

	if (zend_get_configuration_directive_wrapper(ITEM_NAME_BASE_PATH, \
			sizeof(ITEM_NAME_BASE_PATH), &base_path) != SUCCESS)
	{
		strcpy(g_fdfs_base_path, "/tmp");
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"fastdht_client.ini does not have item " \
			"\"%s\", set to %s!", __LINE__, 
			ITEM_NAME_BASE_PATH, g_fdfs_base_path);
	}
	else
	{
		snprintf(g_fdfs_base_path, sizeof(g_fdfs_base_path), "%s", \
			Z_STRVAL_P(base_path));
		chopPath(g_fdfs_base_path);
	}

	if (!fileExists(g_fdfs_base_path))
	{
		logError("\"%s\" can't be accessed, error info: %s", \
			g_fdfs_base_path, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}
	if (!isDir(g_fdfs_base_path))
	{
		logError("\"%s\" is not a directory!", g_fdfs_base_path);
		return ENOTDIR;
	}

	if (zend_get_configuration_directive_wrapper(ITEM_NAME_CONNECT_TIMEOUT, \
			sizeof(ITEM_NAME_CONNECT_TIMEOUT), \
			&connect_timeout) == SUCCESS)
	{
		g_fdfs_connect_timeout = atoi(Z_STRVAL_P(connect_timeout));
		if (g_fdfs_connect_timeout <= 0)
		{
			g_fdfs_connect_timeout = DEFAULT_CONNECT_TIMEOUT;
		}
	}
	else
	{
		g_fdfs_connect_timeout = DEFAULT_CONNECT_TIMEOUT;
	}

	if (zend_get_configuration_directive_wrapper(ITEM_NAME_NETWORK_TIMEOUT, \
			sizeof(ITEM_NAME_NETWORK_TIMEOUT), \
			&network_timeout) == SUCCESS)
	{
		g_fdfs_network_timeout = atoi(Z_STRVAL_P(network_timeout));
		if (g_fdfs_network_timeout <= 0)
		{
			g_fdfs_network_timeout = DEFAULT_NETWORK_TIMEOUT;
		}
	}
	else
	{
		g_fdfs_network_timeout = DEFAULT_NETWORK_TIMEOUT;
	}

	if (zend_get_configuration_directive_wrapper(ITEM_NAME_LOG_LEVEL, \
			sizeof(ITEM_NAME_LOG_LEVEL), \
			&log_level) == SUCCESS)
	{
		set_log_level(Z_STRVAL_P(log_level));
	}

	if (zend_get_configuration_directive_wrapper(ITEM_NAME_LOG_FILENAME, \
			sizeof(ITEM_NAME_LOG_FILENAME), \
			&log_filename) == SUCCESS)
	{
		if (Z_STRLEN_P(log_filename) > 0)
		{
			log_set_filename(Z_STRVAL_P(log_filename));
		}
	}

	if (zend_get_configuration_directive_wrapper(ITEM_NAME_ANTI_STEAL_SECRET_KEY, \
			sizeof(ITEM_NAME_ANTI_STEAL_SECRET_KEY), \
			&anti_steal_secret_key) == SUCCESS)
	{
		pAntiStealSecretKey = Z_STRVAL_P(anti_steal_secret_key);
	}
	else
	{
		pAntiStealSecretKey = "";
	}
	buffer_strcpy(&g_anti_steal_secret_key, pAntiStealSecretKey);

	config_list = (FDFSConfigInfo *)malloc(sizeof(FDFSConfigInfo) * \
			config_count);
	if (config_list == NULL)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"malloc %d bytes fail!\n",\
			__LINE__, (int)sizeof(FDFSConfigInfo) * config_count);
		return errno != 0 ? errno : ENOMEM;
	}

	pConfigEnd = config_list + config_count;
	for (pConfigInfo=config_list; pConfigInfo<pConfigEnd; pConfigInfo++)
	{
		nItemLen = sprintf(szItemName, "%s%d", ITEM_NAME_CONF_FILE, \
				(int)(pConfigInfo - config_list));
		if (zend_get_configuration_directive_wrapper(szItemName, \
			nItemLen + 1, &conf_filename) != SUCCESS)
		{
			if (pConfigInfo != config_list)
			{
				fprintf(stderr, "file: "__FILE__", line: %d, " \
					"fastdfs_client.ini: get param %s " \
					"fail!\n", __LINE__, szItemName);

				return ENOENT;
			}

			if (zend_get_configuration_directive_wrapper( \
				ITEM_NAME_CONF_FILE, \
				sizeof(ITEM_NAME_CONF_FILE), \
				&conf_filename) != SUCCESS)
			{
				fprintf(stderr, "file: "__FILE__", line: %d, " \
					"fastdfs_client.ini: get param %s " \
					"fail!\n",__LINE__,ITEM_NAME_CONF_FILE);

				return ENOENT;
			}
		}

		if (pConfigInfo == config_list) //first config file
		{
			pConfigInfo->pTrackerGroup = &g_tracker_group;
		}
		else
		{
			pConfigInfo->pTrackerGroup = (TrackerServerGroup *)malloc( \
							sizeof(TrackerServerGroup));
			if (pConfigInfo->pTrackerGroup == NULL)
			{
				fprintf(stderr, "file: "__FILE__", line: %d, " \
					"malloc %d bytes fail!\n", \
					__LINE__, (int)sizeof(TrackerServerGroup));
				return errno != 0 ? errno : ENOMEM;
			}
		}

		if ((result=fdfs_load_tracker_group(pConfigInfo->pTrackerGroup, 
				Z_STRVAL_P(conf_filename))) != 0)
		{
			return result;
		}
	}

	if (zend_get_configuration_directive_wrapper(ITEM_NAME_USE_CONN_POOL,
		sizeof(ITEM_NAME_USE_CONN_POOL), &use_conn_pool) == SUCCESS)
	{
		char *use_conn_pool_str;

		use_conn_pool_str = Z_STRVAL_P(use_conn_pool);
		if (strcasecmp(use_conn_pool_str, "yes") == 0 || 
			strcasecmp(use_conn_pool_str, "on") == 0 ||
			strcasecmp(use_conn_pool_str, "true") == 0 ||
			strcmp(use_conn_pool_str, "1") == 0)
		{
			if (zend_get_configuration_directive_wrapper( \
				ITEM_NAME_CONN_POOL_MAX_IDLE_TIME, \
				sizeof(ITEM_NAME_CONN_POOL_MAX_IDLE_TIME), \
				&conn_pool_max_idle_time) == SUCCESS)
			{
			g_connection_pool_max_idle_time = \
				atoi(Z_STRVAL_P(conn_pool_max_idle_time));
			if (g_connection_pool_max_idle_time <= 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"%s: %d in config filename" \
					"is invalid!", __LINE__, \
					ITEM_NAME_CONN_POOL_MAX_IDLE_TIME, \
					g_connection_pool_max_idle_time);
				return EINVAL;
			}
			}
			else
			{
				g_connection_pool_max_idle_time = 3600;
			}

			g_use_connection_pool = true;
			result = conn_pool_init(&g_connection_pool, \
					g_fdfs_connect_timeout, \
					0, g_connection_pool_max_idle_time);
			if (result != 0)
			{
				return result;
			}
		}
	}

	logDebug("base_path=%s, connect_timeout=%d, network_timeout=%d, " \
		"anti_steal_secret_key length=%d, " \
		"tracker_group_count=%d, first tracker group server_count=%d, "\
		"use_connection_pool=%d, connection_pool_max_idle_time: %d", \
		g_fdfs_base_path, g_fdfs_connect_timeout, \
		g_fdfs_network_timeout, (int)strlen(pAntiStealSecretKey), \
		config_count, g_tracker_group.server_count, \
		g_use_connection_pool, g_connection_pool_max_idle_time);

	return 0;
}

PHP_MINIT_FUNCTION(fastdfs_client)
{
	zend_class_entry ce;

	log_init();
	if (load_config_files() != 0)
	{
		return FAILURE;
	}

#if PHP_MAJOR_VERSION >= 7
	memcpy(&fdfs_object_handlers, zend_get_std_object_handlers(),
            sizeof(zend_object_handlers));
	fdfs_object_handlers.offset = XtOffsetOf(php_fdfs_t, zo);
	fdfs_object_handlers.free_obj = php_fdfs_free_storage;
	fdfs_object_handlers.clone_obj = NULL;
#endif

	INIT_CLASS_ENTRY(ce, "FastDFS", fdfs_class_methods);
	fdfs_ce = zend_register_internal_class(&ce TSRMLS_CC);
	fdfs_ce->create_object = php_fdfs_new;

	INIT_CLASS_ENTRY(ce, "FastDFSException", NULL);
#if PHP_MAJOR_VERSION < 7
	fdfs_exception_ce = zend_register_internal_class_ex(&ce, \
		php_fdfs_get_exception_base(0 TSRMLS_CC), NULL TSRMLS_CC);
#else
	fdfs_exception_ce = zend_register_internal_class_ex(&ce, \
		php_fdfs_get_exception_base(0 TSRMLS_CC));
#endif

	REGISTER_STRING_CONSTANT("FDFS_FILE_ID_SEPERATOR", \
			FDFS_FILE_ID_SEPERATE_STR, \
			CONST_CS | CONST_PERSISTENT);

	REGISTER_STRING_CONSTANT("FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE", \
			STORAGE_SET_METADATA_FLAG_OVERWRITE_STR, \
			CONST_CS | CONST_PERSISTENT);

	REGISTER_STRING_CONSTANT("FDFS_STORAGE_SET_METADATA_FLAG_MERGE", \
			STORAGE_SET_METADATA_FLAG_MERGE_STR, \
			CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("FDFS_STORAGE_STATUS_INIT", \
		FDFS_STORAGE_STATUS_INIT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FDFS_STORAGE_STATUS_WAIT_SYNC", \
		FDFS_STORAGE_STATUS_WAIT_SYNC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FDFS_STORAGE_STATUS_SYNCING", \
		FDFS_STORAGE_STATUS_SYNCING, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FDFS_STORAGE_STATUS_DELETED", \
		FDFS_STORAGE_STATUS_DELETED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FDFS_STORAGE_STATUS_OFFLINE", \
		FDFS_STORAGE_STATUS_OFFLINE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FDFS_STORAGE_STATUS_ONLINE", \
		FDFS_STORAGE_STATUS_ONLINE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FDFS_STORAGE_STATUS_ACTIVE", \
		FDFS_STORAGE_STATUS_ACTIVE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FDFS_STORAGE_STATUS_NONE", \
		FDFS_STORAGE_STATUS_NONE, CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(fastdfs_client)
{
	FDFSConfigInfo *pConfigInfo;
	FDFSConfigInfo *pConfigEnd;

	if (config_list != NULL)
	{
		pConfigEnd = config_list + config_count;
		for (pConfigInfo=config_list; pConfigInfo<pConfigEnd; \
			pConfigInfo++)
		{
			if (pConfigInfo->pTrackerGroup != NULL)
			{
				tracker_close_all_connections_ex( \
						pConfigInfo->pTrackerGroup);
			}
		}
	}

	if (g_use_connection_pool)
	{
		fdfs_connection_pool_destroy();
	}

	fdfs_client_destroy();
	log_destroy();

	return SUCCESS;
}

PHP_RINIT_FUNCTION(fastdfs_client)
{
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(fastdfs_client)
{
	fprintf(stderr, "request shut down. file: "__FILE__", line: %d\n", __LINE__);
	return SUCCESS;
}

PHP_MINFO_FUNCTION(fastdfs_client)
{
	char fastdfs_info[64];
	sprintf(fastdfs_info, "fastdfs_client v%d.%02d support", 
		g_fdfs_version.major, g_fdfs_version.minor);

	php_info_print_table_start();
	php_info_print_table_header(2, fastdfs_info, "enabled");
	php_info_print_table_end();
}

