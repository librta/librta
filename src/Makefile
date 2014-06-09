################################################################
# Makefile -- to make the rta library.
#    Add or remove debug stuff to meet your needs.  
#    The "standard" target runs the source through indent to
# give the source a standardized look.
################################################################
# We use the default make rules for .c and .l 

CC = gcc
OPT =  -fPIC -c -I.
DEBUG = -g -DDEBUG -DYYDEBUG -ggdb -Wall
#DEBUG = -g -DYYDEBUG -ggdb -Wall
CFLAGS = $(OPT) $(DEBUG)
OBJS   = api.o token.o parse.tab.o do_sql.o rtatables.o

librtadb.so.1: $(OBJS)
	gcc -ggdb -Wall -shared -Wl,-soname,librtadb.so.1 \
		-o librtadb.so.1.0  $(OBJS) -lc
	ln -sf librtadb.so.1.0 librtadb.so.1
	ln -sf librtadb.so.1.0 librtadb.so
	ar -rcs librtadb.a $(OBJS)

token.o: parse.tab.c do_sql.h

parse.tab.c: do_sql.h
	yacc -dv -bparse parse.y

api.o: api.c rta.h do_sql.h

do_sql.o: do_sql.c do_sql.h rta.h

rtatables.o: rtatables.c do_sql.h rta.h

standard: clean
	sh ../util/pprint.sh

clean: 
	rm -rf *.o parse.tab.? token.c librtadb*

