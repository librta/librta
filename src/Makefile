################################################################
 # Run Time Access
 # Copyright (C) 2003-2006 Robert W Smith (bsmith@linuxtoys.org)
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
DEBUG = -g -ggdb -DDEBUG -DYYDEBUG -ggdb -Wall
CFLAGS = $(OPT) $(DEBUG)
OBJS   = api.o token.o parse.tab.o do_sql.o rtatables.o
LIBS   = 



default: librtadb.so.2

librtadb.so.2: $(OBJS)
	gcc -ggdb -Wall -shared -Wl,-soname,librtadb.so.2 \
		-o librtadb.so.2.0  $(OBJS) $(LIBS) -lc
	ln -sf librtadb.so.2.0 librtadb.so.2
	ln -sf librtadb.so.2.0 librtadb.so
	ar -rcs librtadb.a $(OBJS)


token.o: parse.tab.c do_sql.h

parse.tab.c: parse.y do_sql.h
	yacc -dv -bparse parse.y

api.o: api.c rta.h do_sql.h

do_sql.o: do_sql.c do_sql.h rta.h

rtatables.o: rtatables.c do_sql.h rta.h

standard: clean
	for i in *.h *.c ; \
	do \
	 indent  -bad -bap -nbbb -br -cdw -cli2 -cbi0 -saf -nut \
		-sai -saw -di9 -nbc -bfda -bls -lp -ci2 -i2 -nbbo -l72 \
		-fca -nbfda -nbfde -nlp -bbo -sob -cd24 -ts0 -npcs -nut \
		-T TBLDEF -T COLDEF $$i ; \
	 rm $$i~ ; \
	done

install:	
	/usr/bin/install -m 644 librta* /usr/local/lib;
	/usr/bin/install -m 644 rta.h /usr/local/include
	ln -sf /usr/local/lib/librtadb.so.2.0 /usr/local/lib/librtadb.so.2
	ln -sf /usr/local/lib/librtadb.so.2.0 /usr/local/lib/librtadb.so

uninstall:	
	rm /usr/local/lib/librta*;
	rm /usr/local/include/rta.h

clean: 
	rm -rf *.o parse.tab.? token.c lib* parse.output parse.save *~

