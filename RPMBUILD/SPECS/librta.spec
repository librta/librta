# Spec file for librta

%define _base           %(echo "$BASE")
%define _topdir	 	%{_base}/RPMBUILD
%define name		librta
%define version 	1.1.0
%define release		1
%define buildroot	%{_topdir}/ROOT
%define prefix		/usr

BuildRoot:		%{buildroot}
Summary: 		Run Time Access Library
Vendor:                 Demand Peripherals
URL:			http://www.librta.org
Packager:		Frederic Roussel <fr.frasc@gmail.com>
License: 		LGPL-2.1+
Name: 			%{name}
Version: 		%{version}
Release: 		%{release}
Source: 		%{name}-%{version}.tar.gz
Prefix: 		%{prefix}
Group: 			System Environment/Libraries

%description
LIBRTA gives you run time access to the data in your program.
It is intended for embedded system developers and can
greatly simplify user-interface programs by separating the
daemon proper from the UI programs.

%package dev
Summary:                Run Time Access Library
Group:                  System Environment/Libraries
%description dev
Run Time Access Library - development files

%package doc
Summary:                Run Time Access Library
Group:                  System Environment/Libraries
%description doc
Run Time Access Library - documentation files

%package examples
Summary:                Run Time Access Library
Group:                  System Environment/Libraries
%description examples
Run Time Access Library - examples

%prep
mkdir -p %{buildroot}
mkdir -p %{buildroot}/ROOT
mkdir -p %{buildroot}/BUILD
mkdir -p %{buildroot}/RPMS
mkdir -p %{buildroot}/SOURCES
mkdir -p %{buildroot}/SRPMS

%build
cd %{_base}/src; make

%install
cd %{_base}/src; make install INSTDIR=%{buildroot}/%{prefix}
mkdir -p %{buildroot}/usr/share/doc/librta-examples
cp -a %{_base}/test %{buildroot}/usr/share/doc/librta-examples
cp -a %{_base}/table_editor %{buildroot}/usr/share/doc/librta-examples
mkdir -p %{buildroot}/usr/share/doc/librta-doc/html
cp -a %{_base}/doc/* %{buildroot}/usr/share/doc/librta-doc/html

%post
ldconfig

%postun
ldconfig

%files
%defattr(-,root,root)
/usr/lib/librta.so.3
/usr/lib/librta.so.3.0
/usr/lib/librta.so

%files dev
%defattr(-,root,root)
/usr/lib/librta.a
/usr/include/librta.h

%files doc
/usr/share/doc/librta-doc/html/BadUnixModel.png
/usr/share/doc/librta-doc/html/FAQ.html
/usr/share/doc/librta-doc/html/GoodrtaModel.png
/usr/share/doc/librta-doc/html/apiref.html
/usr/share/doc/librta-doc/html/contact.html
/usr/share/doc/librta-doc/html/contact.php
/usr/share/doc/librta-doc/html/download.html
/usr/share/doc/librta-doc/html/index.html
/usr/share/doc/librta-doc/html/librta_client.c
/usr/share/doc/librta-doc/html/librta_client.txt
/usr/share/doc/librta-doc/html/livedemo.html
/usr/share/doc/librta-doc/html/myappdb.c
/usr/share/doc/librta-doc/html/quickstart.html

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

