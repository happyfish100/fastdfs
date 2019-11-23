
if [ -n "$1" ]; then
  TARGET_CONF_PATH=$1
else
  TARGET_CONF_PATH=/etc/fdfs
fi

mkdir -p $TARGET_CONF_PATH

if [ ! -f $TARGET_CONF_PATH/tracker.conf ]; then
  cp -f conf/tracker.conf $TARGET_CONF_PATH/tracker.conf
fi

if [ ! -f $TARGET_CONF_PATH/storage.conf ]; then
  cp -f conf/storage.conf $TARGET_CONF_PATH/storage.conf
fi

if [ ! -f $TARGET_CONF_PATH/client.conf ]; then
  cp -f conf/client.conf $TARGET_CONF_PATH/client.conf
fi

if [ ! -f $TARGET_CONF_PATH/storage_ids.conf ]; then
  cp -f conf/storage_ids.conf $TARGET_CONF_PATH/storage_ids.conf
fi

if [ ! -f $TARGET_CONF_PATH/http.conf ]; then
  cp -f conf/http.conf $TARGET_CONF_PATH/http.conf
fi

if [ ! -f $TARGET_CONF_PATH/mime.types ]; then
  cp -f conf/mime.types $TARGET_CONF_PATH/mime.types
fi
