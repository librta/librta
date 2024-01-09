###################################################################
 # librta library
 # Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 #
 #  This program is distributed under the terms of the MIT License.
 #  See the file COPYING file.
###################################################################

################################################################
# Makefile -- to make the librta library.
#    Add or remove debug stuff to meet your needs.  
#    The "standard" target runs the source through indent to
# give the source a standardized look.
################################################################
# We use the default make rules for .c and .l 

# Configurable items: to be commented out or changed as needed.
# STRINGS_OPT = -DUSE_BUILTIN_STRNDUP
YACC = yacc
LEX = flex

# CPU architecture for the build host.
HOST_ARCH := $(shell uname -m)
# Normalized CPU architecture for the target.
PKG_ARCH = $(TGT_ARCH:i%86=i386)

# System the build is running on.
SYS := $(shell uname)

CC    ?= gcc
#DEBUG ?= -DDEBUG -DYYDEBUG
OPT    = -fPIC -Wall -D_GNU_SOURCE $(STRINGS_OPT)
ifdef CFLAGS
  CFLAGS += $(OPT) $(DEBUG)
else
  CFLAGS = -g -O2 $(OPT) $(DEBUG)
endif

# Add flags to cross compile, depending on the bitness.
ifeq ($(PKG_ARCH), i386)
  ifeq ($(HOST_ARCH), x86_64)
    CC += -m32
  endif
else ifeq ($(PKG_ARCH), x86_64)
  ifeq ($(HOST_ARCH), i386)
    CC += -m64
  endif
endif

OBJS   = api.o token.o parse.tab.o do_sql.o rtatables.o
LIBS   = 

INSTDIR    ?= /usr/local
INSTLIBDIR ?= $(INSTDIR)/lib
INSTINCDIR ?= $(INSTDIR)/include

default: librta.so.3 librta.a

ifeq ($(SYS), Darwin)
# TO FIX
# of course the ../src rpath is for internal development only.
librta.so.3: $(OBJS)
	$(CC) -g -Wall -dynamiclib -install_name @rpath/../src/librta.dynlib \
		-o librta.dynlib $(OBJS)
else
librta.so.3: $(OBJS)
	$(CC) -g -Wall -shared -Wl,-soname,librta.so.3 \
		-o librta.so.3.0 $(OBJS)
	ln -sf librta.so.3.0 librta.so.3
	ln -sf librta.so.3 librta.so
endif

librta.a: $(OBJS)
	ar -rcs librta.a $(OBJS)

token.o: parse.tab.c do_sql.h

parse.tab.c: parse.y do_sql.h
	$(YACC) -dv -bparse -prta_yy parse.y

token.c: token.l
	$(LEX) -Prta_yy -t token.l > token.c

api.o: api.c librta.h do_sql.h

do_sql.o: do_sql.c do_sql.h librta.h

rtatables.o: rtatables.c do_sql.h librta.h

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
	ln -sf librta.so.3.0 $(DESTDIR)/$(INSTLIBDIR)/librta.so.3

# the librta.so symlink is needed only for development.
install-dev: default
	-mkdir -p $(DESTDIR)/$(INSTLIBDIR)
	-mkdir -p $(DESTDIR)/$(INSTINCDIR)
	install -m 644 librta*.a $(DESTDIR)/$(INSTLIBDIR)
	install -m 644 librta.h $(DESTDIR)/$(INSTINCDIR)
	ln -sf librta.so.3 $(DESTDIR)/$(INSTLIBDIR)/librta.so

uninstall:
	rm $(DESTDIR)/$(INSTLIBDIR)/librta*
	rm $(DESTDIR)/$(INSTINCDIR)/librta.h

clean: 
	rm -rf *.o parse.tab.? token.c parse.output parse.save *~
	rm -f lib*.a lib*.so*

