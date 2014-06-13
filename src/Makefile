################################################################
 # Run Time Access
 # Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 #
 #  This program is distributed under the terms of the GNU LGPL.
 #  See the file COPYING file.
################################################################

################################################################
# Makefile -- to make the rta library.
#    Add or remove debug stuff to meet your needs.  
#    The "standard" target runs the source through indent to
# give the source a standardized look.
################################################################
# We use the default make rules for .c and .l 

CC = gcc
OPT =  -fPIC -c -I.
DEBUG = -g -O -ggdb -DDEBUG -DYYDEBUG -ggdb -Wall
CFLAGS = $(OPT) $(DEBUG) -D_GNU_SOURCE
OBJS   = api.o token.o parse.tab.o do_sql.o rtatables.o
LIBS   = 

INSTDIR ?= /usr/local
INSTLIBDIR ?= $(INSTDIR)/lib
INSTINCDIR ?= $(INSTDIR)/include


default: librtadb.so.3 librtadb.a

librtadb.so.3: $(OBJS)
	gcc -ggdb -Wall -shared -Wl,-soname,librtadb.so.3 \
		-o librtadb.so.3.0  $(OBJS) $(LIBS) -lc
	ln -sf librtadb.so.3.0 librtadb.so.3
	ln -sf librtadb.so.3.0 librtadb.so
	ar -rcs librtadb.a $(OBJS)

librtadb.a: $(OBJS)
	ar -rcs librtadb.a $(OBJS)

token.o: parse.tab.c do_sql.h

parse.tab.c: parse.y do_sql.h
	yacc -dv -bparse -prta_yy parse.y

token.c: token.l
	lex -Prta_yy -t token.l > token.c

api.o: api.c rta.h do_sql.h

do_sql.o: do_sql.c do_sql.h rta.h

rtatables.o: rtatables.c do_sql.h rta.h

standard: clean
	for i in *.h *.c ; \
	do \
	 indent  -bad -bap -nbbb -br -cdw -cli2 -cbi0 -saf -nut \
		-sai -saw -di9 -nbc -bfda -bls -lp -ci2 -i2 -nbbo -l72 \
		-fca -nbfda -nbfde -nlp -bbo -sob -cd24 -ts0 -npcs -nut \
		-T RTA_TBLDEF -T RTA_COLDEF $$i ; \
	 rm $$i~ ; \
	done

install: default
	-mkdir -p $(DESTDIR)/$(INSTLIBDIR)
	-mkdir -p $(DESTDIR)/$(INSTINCDIR)
	install -m 644 librta* $(DESTDIR)/$(INSTLIBDIR);
	install -m 644 rta.h $(DESTDIR)/$(INSTINCDIR)
	-test -f librtadb.so.3 && ln -sf \
	    $(DESTDIR)/$(INSTLIBDIR)/librtadb.so.3.0 \
	    $(DESTDIR)/$(INSTLIBDIR)/librtadb.so.3
	-test -f librtadb.so.3 && ln -sf \
	    $(DESTDIR)/$(INSTLIBDIR)/librtadb.so.3.0 \
	    $(DESTDIR)/$(INSTLIBDIR)/librtadb.so

uninstall:
	rm $(DESTDIR)/$(INSTLIBDIR)/librta*;
	rm $(DESTDIR)/$(INSTINCDIR)/rta.h

clean: 
	rm -rf *.o parse.tab.? token.c lib* parse.output parse.save *~

