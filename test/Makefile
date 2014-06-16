################################################################
 # Run Time Access Library
 # Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 #
 #  This program is distributed under the terms of the GNU LGPL.
 #  See the file COPYING file.
################################################################

################################################################
# Makefile -- to make the sample application for librta.
#    Add or remove debug stuff to meet your needs.  
#    The "standard" target runs the source through indent to
# give the source a standardized look.
################################################################
# We use the default make rules for .c and .l 

# If you installed this Makefile using a pre-built package
# then 'DEV' will be false.
DEV := $(shell [ -f ../src/librta.h ] || echo false)

ifeq ($(DEV), false)
  # variables set if installed with a package.
  OPT       := -c
  LDOPTS    := -lrta
  STATICLIB := /usr/lib/librta.a
else
  # for library internal development.
  OPT       := -c -I../src
  LDOPTS    := -L../src -Wl,-rpath=../src -lrta
  STATICLIB := ../src/librta.a
endif

CC        = gcc
DEBUG     = -g -DDEBUG -Wall
CFLAGS    = $(OPT) $(DEBUG)
APPOBJS   = appmain.o apptables.o

all: app app-static librta_client

app: $(APPOBJS) 
	$(CC) $(APPOBJS) -g -o $@ $(LDOPTS)

app-static: $(APPOBJS) 
	$(CC) $(APPOBJS) -g -o $@ $(STATICLIB)

librta_client: librta_client.o
	$(CC) -g librta_client.c -o $@ -lpq

appmain.o: appmain.c app.h
	$(CC) $(CFLAGS) $< -o $@

apptables.o: apptables.c app.h
	$(CC) $(CFLAGS) $< -o $@

standard: clean
	for i in *.h *.c ; \
	do \
	 indent  -bad -bap -nbbb -bli0 -cdw -cli2 -cbi0 -saf -nut \
		-sai -saw -di9 -nbc -bfda -bls -lp -ci2 -i2 -nbbo -l72 \
		-fca -nbfda -nbfde -nlp -bbo -sob -cd24 -ts0 -npcs -nut \
		-T RTA_TBLDEF -T RTA_COLDEF $$i ; \
	 rm $$i~ ; \
	done

clean: 
	rm -rf *.o app app-static librta_client

