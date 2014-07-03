# Spec file for librta

%define _base           %(echo "BASE")
%define _stage	 	%(echo "$ROOT")
%define _topdir	 	%{_base}/RPMBUILD
%define name		librta
%define version 	1.1.0
%define release		1
%define buildroot	%{_topdir}/ROOT

BuildRoot:		%{buildroot}
Summary: 		librta
Vendor:                 Demand Peripherals
License: 		LGPL-2.1+
Name: 			%{name}
Version: 		%{version}
Release: 		%{release}
Source: 		%{name}-%{version}.tar.gz
Prefix: 		/usr
Group: 			System Environment/Libraries

%description
Run Time Access Library.

%package dyn
Summary:                librta
Group:                  System Environment/Libraries
%description dyn
Run Time Access Library.

%prep

%build

%install
mkdir -p %{buildroot}/usr/lib
cp -a %{_stage}/usr/lib/libCOMClient* %{buildroot}/usr/lib
mkdir -p %{buildroot}/usr/include
cp -a %{_stage}/usr/include/libcomclient.h %{buildroot}/usr/include

%debug_package

%post
ldconfig

%postun
ldconfig

%files dyn
%defattr(-,root,root)
/usr/lib/libCOMClient.so
/usr/lib/libCOMClient.so.1
/usr/lib/libCOMClient.so.1.0.0
/usr/include/libcomclient.h

%files
%defattr(-,root,root)
/usr/lib/libCOMClient.a
/usr/include/libcomclient.h

