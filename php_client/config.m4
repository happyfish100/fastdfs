dnl config.m4 for extension fastdfs_client

PHP_ARG_WITH(fastdfs_client, for fastdfs_client support FastDFS client,
[  --with-fastdfs_client             Include fastdfs_client support FastDFS client])

if test "$PHP_FASTDFS_CLIENT" != "no"; then
  PHP_SUBST(FASTDFS_CLIENT_SHARED_LIBADD)

  if test -z "$ROOT"; then
	ROOT=/usr
  fi

  PHP_ADD_INCLUDE($ROOT/include/fastcommon)
  PHP_ADD_INCLUDE($ROOT/include/fastdfs)

  PHP_ADD_LIBRARY_WITH_PATH(fastcommon, $ROOT/lib, FASTDFS_CLIENT_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(fdfsclient, $ROOT/lib, FASTDFS_CLIENT_SHARED_LIBADD)

  PHP_NEW_EXTENSION(fastdfs_client, fastdfs_client.c, $ext_shared)

  CFLAGS="$CFLAGS -Wall"
fi
