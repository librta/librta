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

CC    ?= gcc
DEBUG ?= -DDEBUG -DYYDEBUG
OPT    = -fPIC -Wall -D_GNU_SOURCE
ifdef CFLAGS
  CFLAGS += $(OPT) $(DEBUG)
else
  CFLAGS = -g -O2 $(OPT) $(DEBUG)
endif

OBJS   = api.o token.o parse.tab.o do_sql.o rtatables.o
LIBS   = 

INSTDIR    ?= /usr/local
INSTLIBDIR ?= $(INSTDIR)/lib
INSTINCDIR ?= $(INSTDIR)/include

default: librtadb.so.3 librtadb.a

librtadb.so.3: $(OBJS)
	$(CC) -g -Wall -shared -Wl,-soname,librtadb.so.3 \
		-o librtadb.so.3.0 $(OBJS)
	ln -sf librtadb.so.3.0 librtadb.so.3
	ln -sf librtadb.so.3.0 librtadb.so

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
	for i in *.h *.c ;                                              \
	do                                                              \
	 indent  -bad -bap -nbbb -br -cdw -cli2 -cbi0 -saf -nut         \
		-sai -saw -di9 -nbc -bfda -bls -lp -ci2 -i2 -nbbo -l72  \
		-fca -nbfda -nbfde -nlp -bbo -sob -cd24 -ts0 -npcs -nut \
		-T RTA_TBLDEF -T RTA_COLDEF $$i ;                       \
	 rm $$i~ ;                                                      \
	done

install: install-so install-dev

install-so: default
	-mkdir -p $(DESTDIR)/$(INSTLIBDIR)
	install -m 644 librta*.so.3.0 $(DESTDIR)/$(INSTLIBDIR)
	ln -sf librtadb.so.3.0 $(DESTDIR)/$(INSTLIBDIR)/librtadb.so.3
	ln -sf librtadb.so.3.0 $(DESTDIR)/$(INSTLIBDIR)/librtadb.so

install-dev: default
	-mkdir -p $(DESTDIR)/$(INSTLIBDIR)
	-mkdir -p $(DESTDIR)/$(INSTINCDIR)
	install -m 644 librta*.a $(DESTDIR)/$(INSTLIBDIR)
	install -m 644 rta.h $(DESTDIR)/$(INSTINCDIR)

uninstall:
	rm $(DESTDIR)/$(INSTLIBDIR)/librta*
	rm $(DESTDIR)/$(INSTINCDIR)/rta.h

clean: 
	rm -rf *.o parse.tab.? token.c lib* parse.output parse.save *~

