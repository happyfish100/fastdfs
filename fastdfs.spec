%define FastDFS    fastdfs
%define FDFSServer fastdfs-server
%define FDFSClient libfdfsclient
%define FDFSTool   fastdfs-tool
%define FDFSVersion 5.0.6

Name: %{FastDFS}
Version: %{FDFSVersion}
Release: 1%{?dist}
Summary: FastDFS server and client
License: GPL
Group: Arch/Tech
URL: 	http://perso.orange.fr/sebastien.godard/
Source: http://perso.orange.fr/sebastien.godard/%{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n) 

Requires: %__cp %__mv %__chmod %__grep %__mkdir %__install %__id

%description
This package provides tracker & storage of fastdfs

%package -n %{FDFSServer}
Requires: libfastcommon >= 1.0.14
Summary: fastdfs tracker & storage

%package -n %{FDFSTool}
Requires: libfastcommon
Summary: fastdfs tools

%package -n %{FDFSClient}
Requires: libfastcommon
Summary: The client dynamic library of fastdfs

%package -n %{FDFSClient}-devel
Requires: %{FDFSClient}
Summary: The client header of fastdfs

%description -n %{FDFSServer}
This package provides tracker & storage of fastdfs

%description -n %{FDFSClient}
This package is client dynamic library of fastdfs

%description -n %{FDFSClient}-devel
This package is client header of fastdfs client

%description -n %{FDFSTool}
This package is tools for fastdfs

%prep
%setup -q

%build
# FIXME: I need to fix the upstream Makefile to use LIBDIR et al. properly and
# send the upstream maintainer a patch.
# add DOCDIR to the configure part
./make.sh

%install
rm -rf %{buildroot}
DESTDIR=$RPM_BUILD_ROOT ./make.sh install
#make install IGNORE_MAN_GROUP=y DOC_DIR=%{_docdir}/%{name}-%{version} INIT_DIR=%{_initrddir}

#install -m 0644 sysstat.crond %{buildroot}/%{_sysconfdir}/cron.d/sysstat

#%find_lang %{name}

%post -n %{FDFSServer}
/sbin/chkconfig --add fdfs_trackerd
/sbin/chkconfig --add fdfs_storaged

%preun -n %{FDFSServer}
/sbin/chkconfig --del fdfs_trackerd
/sbin/chkconfig --del fdfs_storaged

%postun

%clean
#rm -rf %{buildroot}

%files
#%defattr(-,root,root,-)
#/usr/local/bin/*
#/usr/local/include/*

%files -n %{FDFSServer}
%defattr(-,root,root,-)
/usr/bin/fdfs_trackerd
/usr/bin/fdfs_storaged
/usr/bin/restart.sh
/usr/bin/stop.sh
/etc/init.d/*
/etc/fdfs/tracker.conf.sample
/etc/fdfs/storage.conf.sample

%files -n %{FDFSClient}
/usr/lib64/libfdfsclient*
/etc/fdfs/client.conf.sample

%files -n %{FDFSClient}-devel
%defattr(-,root,root,-)
/usr/include/fastdfs/*

%files -n %{FDFSTool}
/usr/bin/fdfs_monitor
/usr/bin/fdfs_test
/usr/bin/fdfs_test1
/usr/bin/fdfs_crc32
/usr/bin/fdfs_upload_file
/usr/bin/fdfs_download_file
/usr/bin/fdfs_delete_file
/usr/bin/fdfs_file_info
/usr/bin/fdfs_appender_test
/usr/bin/fdfs_appender_test1
/usr/bin/fdfs_append_file
/usr/bin/fdfs_upload_appender

%changelog
* Mon Jun 23 2014  Zaixue Liao <liaozaixue@yongche.com>
- first RPM release (1.0)
