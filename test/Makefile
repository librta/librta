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
OBJS   = appmain.o apptables.o

default: app

app: $(OBJS) 
	$(CC) $(OBJS) -o $@ -L../src -lrtadb

app-static: $(OBJS) 
	$(CC) $(OBJS) -o $@ ../src/api.o  ../src/do_sql.o \
         ../src/parse.tab.o  ../src/rtatables.o  ../src/token.o

appmain.o: appmain.c app.h
	$(CC) $(CFLAGS) $< -o $@

apptables.o: apptables.c app.h
	$(CC) $(CFLAGS) $< -o $@

standard: app
	sh ../misc/pprint.sh

clean: 
	rm -rf *.o app 

