# Spec file for librta

%define _base           %(echo "$BASE")
%define _topdir	 	%{_base}/RPMBUILD
%define name		librta
%define version 	1.1.3.1
%define release		1
%define buildroot	%{_topdir}/ROOT
%define prefix		/usr
%define homepage	http://www.librta.org

BuildRoot:		%{buildroot}
Summary: 		Run Time Access Library
Vendor:                 Robert W Smith <bsmith@linuxtoys.org>
URL:			%{homepage}
Packager:		Frederic Roussel <fr.frasc@gmail.com>
License: 		MIT
Name: 			%{name}
Version: 		%{version}
Release: 		%{release}
Source: 		%{name}-%{version}.tar.gz
Prefix: 		%{prefix}
Group: 			System Environment/Libraries
%if "%{_host_vendor}" != "pc"
BuildRequires:          flex, bison
%endif

%description
LIBRTA gives you run time access to the data in your program.
It is intended for embedded system developers and can
greatly simplify user-interface programs by separating the
daemon proper from the UI programs.

%package devel
Summary:                Run Time Access Library
Group:                  System Environment/Libraries
Requires:		postgresql-devel
%description devel
Run Time Access Library - development files

%package examples
Summary:                Run Time Access Library
Group:                  System Environment/Libraries
Requires:               %{name}-devel
%description examples
Run Time Access Library - examples

%prep

%build
cd %{_base}/src; %{__make} TGT_ARCH=%{_target_cpu}

# the debuginfo must be revisited.
%install
cd %{_base}/src; %{__make} install INSTDIR=%{buildroot}/%{prefix}
cd %{buildroot}/%{prefix}/lib; %{__mkdir} -p debug/%{prefix}/lib; %{__objcopy} --only-keep-debug %{name}.so.3.0 debug/usr/lib/%{name}.so.debug; %{__strip} -g %{name}.so.3.0; %{__objcopy} --add-gnu-debuglink=debug/usr/lib/%{name}.so.debug %{name}.so.3.0
%{__mkdir} -p %{buildroot}/usr/share/doc/librta
%{__cp} %{_base}/COPYING %{_base}/README %{_base}/ChangeLog %{buildroot}/usr/share/doc/librta
%{__mkdir} -p %{buildroot}/usr/share/doc/librta-devel
%{__cp} %{_base}/COPYING %{_base}/README %{_base}/ChangeLog %{buildroot}/usr/share/doc/librta-devel
%{__mkdir} -p %{buildroot}/usr/share/doc/librta-examples
%{__cp} %{_base}/COPYING %{_base}/README %{_base}/ChangeLog %{buildroot}/usr/share/doc/librta-examples
%{__cp} -a %{_base}/test %{buildroot}/usr/share/doc/librta-examples
%{__cp} -a %{_base}/table_editor %{buildroot}/usr/share/doc/librta-examples
%{__mkdir} -p %{buildroot}/usr/share/pkgconfig
%{__cp} %{_base}/data/librta.pc %{buildroot}/usr/share/pkgconfig
%{__sed} -i -e "s|PREFIX|%{prefix}|" -e "s|HOMEPAGE|%{homepage}|" -e "s|VERSION|%{version}|" %{buildroot}/usr/share/pkgconfig/librta.pc

%debug_package

%post
ldconfig

%postun
ldconfig

%files
%defattr(-,root,root)
/usr/lib/librta.so.3
/usr/lib/librta.so.3.0
/usr/lib/librta.so
/usr/share/doc/librta/COPYING
/usr/share/doc/librta/README
/usr/share/doc/librta/ChangeLog

%files devel
%defattr(-,root,root)
/usr/lib/librta.a
/usr/include/librta.h
/usr/share/pkgconfig/librta.pc
/usr/share/doc/librta-devel/COPYING
/usr/share/doc/librta-devel/README
/usr/share/doc/librta-devel/ChangeLog

%files examples
/usr/share/doc/librta-examples/table_editor/rta_apps.html
/usr/share/doc/librta-examples/table_editor/rta_delete.php
/usr/share/doc/librta-examples/table_editor/rta_editadd.php
/usr/share/doc/librta-examples/table_editor/rta_insert.php
/usr/share/doc/librta-examples/table_editor/rta_tables.php
/usr/share/doc/librta-examples/table_editor/rta_update.php
/usr/share/doc/librta-examples/table_editor/rta_view.php
/usr/share/doc/librta-examples/test/Makefile
/usr/share/doc/librta-examples/test/app.h
/usr/share/doc/librta-examples/test/appmain.c
/usr/share/doc/librta-examples/test/apptables.c
/usr/share/doc/librta-examples/test/librta_client.c
/usr/share/doc/librta-examples/COPYING
/usr/share/doc/librta-examples/README
/usr/share/doc/librta-examples/ChangeLog

%clean
cd %{_base}/src; %{__make} clean
%{__rm} -fr %{buildroot}/* %{_topdir}/SOURCES/* %{_topdir}/BUILD/* 

