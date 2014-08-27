#!/bin/sh

export BASE=`pwd`
export TOPDIR=$BASE/RPMBUILD
[ -d $TOPDIR ] || mkdir -p $TOPDIR
# export ROOTDIR=$BASE/root
export ROOTDIR=$TOPDIR/ROOT
[ -d $ROOTDIR ] || mkdir -p $ROOTDIR

for d in BUILD RPMS SOURCES SRPMS; do
  [ -d $TOPDIR/$d ] || mkdir -p $TOPDIR/$d
done

V=`grep ' version' $TOPDIR/SPECS/librta.spec | sed 's/.*version[[:space:]]*//'`

mkdir ../librta-$V
# don't copy over the '.git' tree.
cp -a * ../librta-$V
rm -fr ../librta-$V/{RPMBUILD,SH}
cd ..
tar zcvf $TOPDIR/SOURCES/librta-$V.tar.gz librta-$V
rm -fr librta-$V
cd librta

# build binary and source.
rpmbuild -v -ba --buildroot $TOPDIR/ROOT $TOPDIR/SPECS/librta.spec

# binary only for a given PLATFORM:
# with PLATFORM: arch-vendor-os
rpmbuild -v -bb --target i386-redhat-linux --buildroot $TOPDIR/ROOT $TOPDIR/SPECS/librta.spec

# Move RPMS to a know place
mkdir -p ~/RPMS/SRPMS ~/RPMS/i386 ~/RPMS/x86_64
mv RPMBUILD/SRPMS/* ~/RPMS/SRPMS/
mv RPMBUILD/RPMS/i386/* ~/RPMS/i386/
mv RPMBUILD/RPMS/x86_64/* ~/RPMS/x86_64/

#--- wipe everything out ---
rm -fr $ROOTDIR/* $TOPDIR/{BUILD,RPMS,SOURCES,SRPMS}/*
rm -fr ~/rpmbuild/

#--- Show paths of RPMs ---
find ~/RPMS/ -type f | grep $V

