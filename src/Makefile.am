
AM_LDFLAGS = -shared
AM_YFLAGS =  -d

include_HEADERS = do_sql.h

library_includedir=$(includedir)
library_include_HEADERS = rta.h 

lib_LIBRARIES  = librtadb.a @RTA_OPT_A_LIB@
EXTRA_LIBRARIES = librtafs.a 

lib_LTLIBRARIES = librtadb.la @RTA_OPT_LA_LIB@
EXTRA_LTLIBRARIES = librtafs.la 

# librtadb_a_CFLAGS prevents automake from complaining with 
# "created both with libtool and without"
librtadb_a_CFLAGS = $(AM_CFLAGS)
librtadb_a_SOURCES = parse.y token.l api.c do_sql.c rtatables.c strndup.c dirname.c mkstemp.c
librtadb_la_SOURCES = parse.y token.l api.c do_sql.c rtatables.c strndup.c dirname.c mkstemp.c

# librtafs_a_CFLAGS prevents automake from complaining with 
# "created both with libtool and without"
librtafs_la_LDFLAGS = -rpath '$(libdir)'
librtafs_a_CFLAGS = $(AM_CFLAGS)
librtafs_a_SOURCES = rtafs.c 
librtafs_la_SOURCES = rtafs.c 

CLEANFILES = parse.output

EXTRA_DIST = parse.h parse.y token.l parse.c token.c Makefile.Borland.bcc32
