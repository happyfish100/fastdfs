%define FastDFS    fastdfs
%define FDFSServer fastdfs-server
%define FDFSClient libfdfsclient
%define FDFSClientDevel libfdfsclient-devel
%define FDFSTool   fastdfs-tool
%define FDFSConfig fastdfs-config
%define CommitVersion %(echo $COMMIT_VERSION)

Name: %{FastDFS}
Version: 6.12.3
Release: 1%{?dist}
Summary: FastDFS server and client
License: GPL
Group: Arch/Tech
URL: 	https://github.com/happyfish100/fastdfs/
Source: https://github.com/happyfish100/fastdfs/%{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n) 
BuildRequires: libserverframe-devel >= 1.2.7
Requires: %__cp %__mv %__chmod %__grep %__mkdir %__install %__id
Requires: %{FDFSServer} = %{version}-%{release}
Requires: %{FDFSTool} = %{version}-%{release}

%description
This package provides tracker & storage of fastdfs
commit version: %{CommitVersion}

%package -n %{FDFSServer}
Requires: libserverframe >= 1.2.7
Requires: %{FDFSConfig}
Summary: fastdfs tracker & storage

%package -n %{FDFSTool}
Requires: %{FDFSClient}
Summary: fastdfs tools

%package -n %{FDFSClient}
Requires: libserverframe >= 1.2.7
Requires: %{FDFSConfig}
Summary: The client dynamic library of fastdfs

%package -n %{FDFSClient}-devel
Requires: %{FDFSClient}
Summary: The client header of fastdfs

%package -n %{FDFSConfig}
Summary: FastDFS config files for sample

%description -n %{FDFSServer}
This package provides tracker & storage of fastdfs
commit version: %{CommitVersion}

%description -n %{FDFSClient}
This package is client dynamic library of fastdfs
commit version: %{CommitVersion}

%description -n %{FDFSClient}-devel
This package is client header of fastdfs client
commit version: %{CommitVersion}

%description -n %{FDFSTool}
This package is tools for fastdfs
commit version: %{CommitVersion}

%description -n %{FDFSConfig}
FastDFS config files for sample
commit version: %{CommitVersion}

%prep
%setup -q

%build
./make.sh clean && ./make.sh

%install
rm -rf %{buildroot}
DESTDIR=$RPM_BUILD_ROOT ./make.sh install

%post

%postun

%clean
#rm -rf %{buildroot}

%files

%post -n %{FDFSServer}
systemctl enable fdfs_trackerd
systemctl enable fdfs_storaged

%files -n %{FDFSServer}
%defattr(-,root,root,-)
/usr/bin/fdfs_trackerd
/usr/bin/fdfs_storaged
%config(noreplace) /usr/lib/systemd/system/fdfs_trackerd.service
%config(noreplace) /usr/lib/systemd/system/fdfs_storaged.service

%files -n %{FDFSClient}
%defattr(-,root,root,-)
/usr/lib64/libfdfsclient*
/usr/lib/libfdfsclient*

%files -n %{FDFSClient}-devel
%defattr(-,root,root,-)
/usr/include/fastdfs/*

%files -n %{FDFSTool}
%defattr(-,root,root,-)
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
/usr/bin/fdfs_regenerate_filename

%files -n %{FDFSConfig}
%defattr(-,root,root,-)
%config(noreplace) /etc/fdfs/*.conf

%changelog
* Mon Jun 23 2014  Zaixue Liao <liaozaixue@yongche.com>
- first RPM release (1.0)
