%define FastDFS    fastdfs
%define FDFSServer fastdfs-server
%define FDFSClient libfdfsclient
%define FDFSTool   fastdfs-tool
%define FDFSVersion 5.0.9

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
BuildRequires: libfastcommon-devel >= 1.0.30

%description
This package provides tracker & storage of fastdfs

%package -n %{FDFSServer}
Requires: libfastcommon >= 1.0.30
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
%{__mkdir_p} %{buildroot}/usr/lib/
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
%{_bindir}/fdfs_trackerd
%{_bindir}/fdfs_storaged
%{_bindir}/restart.sh
%{_bindir}/stop.sh
%{_sysconfdir}/init.d/*
%{_sysconfdir}/fdfs/tracker.conf.sample
%{_sysconfdir}/fdfs/storage.conf.sample

%files -n %{FDFSClient}
%{_libdir}/libfdfsclient*
%{_usr}/lib/libfdfsclient*
%{_sysconfdir}/fdfs/client.conf.sample

%files -n %{FDFSClient}-devel
%defattr(-,root,root,-)
%{_includedir}/fastdfs/*

%files -n %{FDFSTool}
%{_bindir}/fdfs_monitor
%{_bindir}/fdfs_test
%{_bindir}/fdfs_test1
%{_bindir}/fdfs_crc32
%{_bindir}/fdfs_upload_file
%{_bindir}/fdfs_download_file
%{_bindir}/fdfs_delete_file
%{_bindir}/fdfs_file_info
%{_bindir}/fdfs_appender_test
%{_bindir}/fdfs_appender_test1
%{_bindir}/fdfs_append_file
%{_bindir}/fdfs_upload_appender

%changelog
* Mon Jun 23 2014  Zaixue Liao <liaozaixue@yongche.com>
- first RPM release (1.0)
