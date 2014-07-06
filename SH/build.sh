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
tar zcvf librta-$V.tar.gz librta-$V
rm -fr librta-$V
mv librta-$V.tar.gz $TOPDIR/SOURCES
cd librta

# binary only.
# rpmbuild -v -bb --buildroot $TOPDIR/ROOT $TOPDIR/SPECS/librta.spec
# rpmbuild -v -bb --noclean --buildroot $TOPDIR/ROOT $TOPDIR/SPECS/librta.spec
# source only.
# rpmbuild -v -bs --buildroot $TOPDIR/ROOT $TOPDIR/SPECS/librta.spec
# binary and source.
rpmbuild -v -ba --buildroot $TOPDIR/ROOT $TOPDIR/SPECS/librta.spec

# binary only for a given PLATFORM:
# with PLATFORM: arch-vendor-os
# rpmbuild -v -bb --target i386-redhat-linux --buildroot $TOPDIR/ROOT $TOPDIR/SPECS/librta.spec

#--- wipe everything out ---
# rm -fr $ROOTDIR/* $TOPDIR/{BUILD,RPMS,SOURCES,SRPMS}/*
# rm -fr ~/rpmbuild/
#
# cd src
# make clean
# cd ..
#
# cd test
# make clean
# cd ..

