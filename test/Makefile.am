
INCLUDES = -I$(top_srcdir)/src/ 

AM_LDFLAGS = -L$(top_srcdir)/src -lrtadb @TEST_OPT_LIBS@ 

bin_PROGRAMS = app

app_SOURCES = appmain.c apptables.c 

EXTRA_DIST = app.h
