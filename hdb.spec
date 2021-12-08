%define version 1.0.1
%define release 1

Name: hdb 
Summary: Hampus Database
Version: %{version} 
Release: %{release} 
Vendor: SXP 
Copyright: LGPL 
Group: Applications/System
Buildroot: /var/tmp/%{name}-%{version}-buildroot
Source: %{name}-%{version}.tar.gz
URL: http://www.hampusdb.org/
Prefix: %{_prefix}
BuildArch: i386
#Requires: xinetd
PreReq: textutils sh-utils

%description
contains database and helper programs

%package devel
Summary: Hampus Database development
Group: Applications/System
Requires: hdb

%description devel
Library and header files used for development

%package -n hdb-perl
Summary: Hampus Database - Perl Modules
Group: Applications/Databases
Release: %{release}
Requires: perl hdb

%description perl
Perl module for accessing Hampus Database 

%prep            
rm -rf %{buildroot}
%setup -q

%build
#./configure --with-db4 --with-linuxthreads --with-network-switch && make
#CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix} && make
#CFLAGS="$RPM_OPT_FLAGS" R=$RPM_BUILD_ROOT ./configure --with-db3 --prefix=%{prefix} && make
CFLAGS="$RPM_OPT_FLAGS" R=$RPM_BUILD_ROOT ./configure --prefix=%{prefix} && make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/etc/init.d/
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
/etc/init.d/*
/etc/profile.d/*

%files devel
%defattr(-,root,root)
%{prefix}/include/*

%files -n hdb-perl
%defattr(-,root,root)
/usr/lib/perl5/site_perl/5.8.5/i386-linux-thread-multi/HDB.pm

%changelog

* Mon Mar 13 2006 Hampus Soderstrom <hampus@sxp.se>
- Initial version
