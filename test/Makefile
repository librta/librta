################################################################
 # Run Time Access
 # Copyright (C) 2003-2004 Robert W Smith (bsmith@linuxtoys.org)
 #
 #  This program is distributed under the terms of the GNU LGPL.
 #  See the file COPYING file.
################################################################

################################################################
# Makefile -- to make the sample application for rta.
#    Add or remove debug stuff to meet your needs.  
#    The "standard" target runs the source through indent to
# give the source a standardized look.
################################################################
# We use the default make rules for .c and .l 

CC = gcc
OPT =  -fPIC -c -I.
DEBUG = -g -DDEBUG -DYYDEBUG -ggdb -Wall
CFLAGS = $(OPT) $(DEBUG)
APPOBJS   = appmain.o apptables.o


default: app

app: $(APPOBJS) 
	$(CC) $(APPOBJS) -o $@ -L../src -lrtadb -lrtafs

app-static: $(APPOBJS) 
	$(CC) $(APPOBJS) -o $@ ../src/librtadb.a ../src/librtafs.a

rta_client: rta_client.o
	$(CC) rta_client.c -o $@ -lpq

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
		-T TBLDEF -T COLDEF -T PRXPORT $$i ; \
	 rm $$i~ ; \
	done

clean: 
	rm -rf *.o app app-static rta_client

