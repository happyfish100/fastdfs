ENABLE_STATIC_LIB=0
ENABLE_SHARED_LIB=1
TARGET_PREFIX=$DESTDIR/usr
TARGET_CONF_PATH=$DESTDIR/etc/fdfs
TARGET_SYSTEMD_PATH=$DESTDIR/usr/lib/systemd/system

WITH_LINUX_SERVICE=1

DEBUG_FLAG=0

export CC=gcc
CFLAGS='-Wall'
GCC_VERSION=$(gcc -dM -E -  < /dev/null | grep -w __GNUC__ | awk '{print $NF;}')
if [ -n "$GCC_VERSION" ] && [ $GCC_VERSION -ge 7 ]; then
  CFLAGS="$CFLAGS -Wformat-truncation=0 -Wformat-overflow=0"
fi
CFLAGS="$CFLAGS -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE"
if [ "$DEBUG_FLAG" = "1" ]; then
  CFLAGS="$CFLAGS -g -O1 -DDEBUG_FLAG"
else
  CFLAGS="$CFLAGS -g -O3"
fi

if [ -f /usr/include/fastcommon/_os_define.h ]; then
  OS_BITS=$(grep -F OS_BITS /usr/include/fastcommon/_os_define.h | awk '{print $NF;}')
elif [ -f /usr/local/include/fastcommon/_os_define.h ]; then
  OS_BITS=$(grep -F OS_BITS /usr/local/include/fastcommon/_os_define.h | awk '{print $NF;}')
else
  OS_BITS=64
fi

uname=$(uname)

if [ "$OS_BITS" -eq 64 ]; then
  if [ $uname = 'Linux' ]; then
    osname=$(cat /etc/os-release | grep -w NAME | awk -F '=' '{print $2;}' | \
            awk -F '"' '{if (NF==3) {print $2} else {print $1}}' | awk '{print $1}')
    if [ $osname = 'Ubuntu' -o $osname = 'Debian' ]; then
      LIB_VERSION=lib
    else
      LIB_VERSION=lib64
    fi
  elif [ "$uname" = "Darwin" ]; then
    LIB_VERSION=lib
  else
    LIB_VERSION=lib64
  fi
else
  LIB_VERSION=lib
fi

LIBS=''


if [ "$uname" = "Linux" ]; then
  if [ "$OS_BITS" -eq 64 ]; then
    LIBS="$LIBS -L/usr/lib64"
  else
    LIBS="$LIBS -L/usr/lib"
  fi
  CFLAGS="$CFLAGS"
elif [ "$uname" = "FreeBSD" ] || [ "$uname" = "Darwin" ]; then
  LIBS="$LIBS -L/usr/lib"
  CFLAGS="$CFLAGS"
  if [ "$uname" = "Darwin" ]; then
    CFLAGS="$CFLAGS -DDARWIN"
    TARGET_PREFIX=$TARGET_PREFIX/local
  fi
elif [ "$uname" = "SunOS" ]; then
  LIBS="$LIBS -L/usr/lib"
  CFLAGS="$CFLAGS -D_THREAD_SAFE"
  LIBS="$LIBS -lsocket -lnsl -lresolv"
  export CC=gcc
elif [ "$uname" = "AIX" ]; then
  LIBS="$LIBS -L/usr/lib"
  CFLAGS="$CFLAGS -D_THREAD_SAFE"
  export CC=gcc
elif [ "$uname" = "HP-UX" ]; then
  LIBS="$LIBS -L/usr/lib"
  CFLAGS="$CFLAGS"
fi

have_pthread=0
if [ -f /usr/lib/libpthread.so ] || [ -f /usr/local/lib/libpthread.so ] || [ -f /lib64/libpthread.so ] || [ -f /usr/lib64/libpthread.so ] || [ -f /usr/lib/libpthread.a ] || [ -f /usr/local/lib/libpthread.a ] || [ -f /lib64/libpthread.a ] || [ -f /usr/lib64/libpthread.a ]; then
  LIBS="$LIBS -lpthread"
  have_pthread=1
elif [ "$uname" = "HP-UX" ]; then
  lib_path="/usr/lib/hpux$OS_BITS"
  if [ -f $lib_path/libpthread.so ]; then
    LIBS="-L$lib_path -lpthread"
    have_pthread=1
  fi
elif [ "$uname" = "FreeBSD" ]; then
  if [ -f /usr/lib/libc_r.so ]; then
    line=$(nm -D /usr/lib/libc_r.so | grep pthread_create | grep -w T)
    if [ $? -eq 0 ]; then
      LIBS="$LIBS -lc_r"
      have_pthread=1
    fi
  elif [ -f /lib64/libc_r.so ]; then
    line=$(nm -D /lib64/libc_r.so | grep pthread_create | grep -w T)
    if [ $? -eq 0 ]; then
      LIBS="$LIBS -lc_r"
      have_pthread=1
    fi
  elif [ -f /usr/lib64/libc_r.so ]; then
    line=$(nm -D /usr/lib64/libc_r.so | grep pthread_create | grep -w T)
    if [ $? -eq 0 ]; then
      LIBS="$LIBS -lc_r"
      have_pthread=1
    fi
  fi
fi

if [ $have_pthread -eq 0 ] && [ "$uname" != "Darwin" ]; then
   /sbin/ldconfig -p | grep -F libpthread.so > /dev/null
   if [ $? -eq 0 ]; then
      LIBS="$LIBS -lpthread"
   else
      echo -E 'Require pthread lib, please check!'
      exit 2
   fi
fi

TRACKER_EXTRA_OBJS=''
STORAGE_EXTRA_OBJS=''
if [ "$DEBUG_FLAG" = "1" ]; then
  TRACKER_EXTRA_OBJS="$TRACKER_EXTRA_OBJS tracker_dump.o"
  STORAGE_EXTRA_OBJS="$STORAGE_EXTRA_OBJS storage_dump.o"
fi

cd tracker
cp Makefile.in Makefile
perl -pi -e "s#\\\$\(CFLAGS\)#$CFLAGS#g" Makefile
perl -pi -e "s#\\\$\(LIBS\)#$LIBS#g" Makefile
perl -pi -e "s#\\\$\(TARGET_PREFIX\)#$TARGET_PREFIX#g" Makefile
perl -pi -e "s#\\\$\(TRACKER_EXTRA_OBJS\)#$TRACKER_EXTRA_OBJS#g" Makefile
perl -pi -e "s#\\\$\(TARGET_CONF_PATH\)#$TARGET_CONF_PATH#g" Makefile
make $1 $2

cd ../storage
cp Makefile.in Makefile
perl -pi -e "s#\\\$\(CFLAGS\)#$CFLAGS#g" Makefile
perl -pi -e "s#\\\$\(LIBS\)#$LIBS#g" Makefile
perl -pi -e "s#\\\$\(TARGET_PREFIX\)#$TARGET_PREFIX#g" Makefile
perl -pi -e "s#\\\$\(STORAGE_EXTRA_OBJS\)#$STORAGE_EXTRA_OBJS#g" Makefile
perl -pi -e "s#\\\$\(TARGET_CONF_PATH\)#$TARGET_CONF_PATH#g" Makefile
make $1 $2

cd ../client
cp Makefile.in Makefile
perl -pi -e "s#\\\$\(CFLAGS\)#$CFLAGS#g" Makefile
perl -pi -e "s#\\\$\(LIBS\)#$LIBS#g" Makefile
perl -pi -e "s#\\\$\(TARGET_PREFIX\)#$TARGET_PREFIX#g" Makefile
perl -pi -e "s#\\\$\(LIB_VERSION\)#$LIB_VERSION#g" Makefile
perl -pi -e "s#\\\$\(TARGET_CONF_PATH\)#$TARGET_CONF_PATH#g" Makefile
perl -pi -e "s#\\\$\(ENABLE_STATIC_LIB\)#$ENABLE_STATIC_LIB#g" Makefile
perl -pi -e "s#\\\$\(ENABLE_SHARED_LIB\)#$ENABLE_SHARED_LIB#g" Makefile

cp fdfs_link_library.sh.in fdfs_link_library.sh
perl -pi -e "s#\\\$\(TARGET_PREFIX\)#$TARGET_PREFIX#g" fdfs_link_library.sh
make $1 $2

cd test
cp Makefile.in Makefile
perl -pi -e "s#\\\$\(CFLAGS\)#$CFLAGS#g" Makefile
perl -pi -e "s#\\\$\(LIBS\)#$LIBS#g" Makefile
perl -pi -e "s#\\\$\(TARGET_PREFIX\)#$TARGET_PREFIX#g" Makefile
cd ..

copy_file()
{
    src=$1
    dest=$2

        if [ ! -f $TARGET_CONF_PATH/tracker.conf ]; then
          cp -f conf/tracker.conf $TARGET_CONF_PATH/tracker.conf
        fi
}

if [ "$1" = "install" ]; then
  cd ..
  if [ "$uname" = "Linux" ]; then
    if [ "$WITH_LINUX_SERVICE" = "1" ]; then
      if [ ! -d $TARGET_CONF_PATH ]; then
        mkdir -p $TARGET_CONF_PATH
        cp -f conf/tracker.conf $TARGET_CONF_PATH/tracker.conf
        cp -f conf/storage.conf $TARGET_CONF_PATH/storage.conf
        cp -f conf/client.conf $TARGET_CONF_PATH/client.conf
        cp -f conf/storage_ids.conf $TARGET_CONF_PATH/storage_ids.conf
        cp -f conf/http.conf $TARGET_CONF_PATH/http.conf
        cp -f conf/mime.types $TARGET_CONF_PATH/mime.types
      fi

      if [ ! -f $TARGET_SYSTEMD_PATH/fdfs_trackerd.service ]; then
        mkdir -p $TARGET_SYSTEMD_PATH
        cp -f systemd/fdfs_trackerd.service $TARGET_SYSTEMD_PATH
        cp -f systemd/fdfs_storaged.service $TARGET_SYSTEMD_PATH
      fi
    fi
  fi
fi
