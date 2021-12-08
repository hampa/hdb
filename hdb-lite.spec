%define version 1.0.1
%define release 1
%define tgzname hdb

Name: hdb-lite
Summary: Hampus Database
Version: %{version} 
Release: %{release} 
Vendor: SXP 
Copyright: LGPL 
Group: Applications/System
Buildroot: /var/tmp/%{tgzname}-%{version}-buildroot
Source: %{tgzname}-%{version}.tar.gz
URL: http://www.hampusdb.org/
Prefix: %{_prefix}
BuildArch: i386
#Requires: xinetd
PreReq: textutils sh-utils

%description
contains database and helper programs for HDB Lite. HDB Lite uses file based access to HampusDB

%package devel
Summary: Hampus Database development
Group: Applications/System
Requires: hdb

%description devel
Library and header files used for development of HDB lite

%prep            
rm -rf %{buildroot}
%setup -n hdb-%{version} -q

%build
CFLAGS="$RPM_OPT_FLAGS" R=$RPM_BUILD_ROOT ./configure --without-hdb-network --prefix=%{prefix} && make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/etc/profile.d/
R=$RPM_BUILD_ROOT make install prefix=$RPM_BUILD_ROOT/%{prefix}

%clean
rm -rf %{buildroot}

%post
mkdir -m 777 -p /var/db/hdb

%files
%defattr(-,root,root)
%{prefix}/lib/*
%{prefix}/bin/*
%{prefix}/man/man3/*
%{prefix}/man/man1/*
/etc/profile.d/*

%files devel
%defattr(-,root,root)
%{prefix}/include/*

%changelog

* Mon May 25 2006 Hampus Soderstrom <hampus@sxp.se>
- Initial version
