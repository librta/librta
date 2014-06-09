###########################################################################
#                        Makefile                                         #
#  copyright            : (C) 2004 by Bob Smith                           #
#                                                                         #
#   This file is the Makefile  for the empty daemon program which         #
# illustrates one way to build a daemon using a poll() or select() and    #
# the Run Time Access package.                                            #
###########################################################################

###########################################################################
#                                                                         #
#   THE AUTHOR RENOUNCES ALL COPYRIGHT TO THIS WORK.                      #
#   This program is free software; you can redistribute it and/or modify  #
#   it for redistribution as you see fit.  Specifically, you may use this #
#   as the basis of your software and you need not make public your       #
#   programs or other work derived from this program.  You may remove     #
#   the above copyright in any derived work.                              #
#                                                                         #
#   However, please let the authors of this program know if you find bugs #
#   or security problems in the program.                                  #
#                                                                         #
###########################################################################

###########################################################################
#  Change Log:                                                            #
#     2004 March 5: Initial release                                       #
###########################################################################

# Makefile Version = "$Id$";


# Targets are:
#  default:      makes empty daemon
#  empd:         empty daemon - does nothing useful
#  clean:        remove unneeded file
#  standard:     does a clean then runs .c and .h file through indent


# We use the default make rules for .c and .l 

CC = gcc
OPT =  -fPIC -c -I.
DEBUG = -g -ggdb -Wall
CFLAGS = $(OPT) $(DEBUG)
EDOBJS   = main.o logit.o select.o ui.o config.o timers.o app.o

default: empd

empd: $(EDOBJS) 
	$(CC) $(EDOBJS) -o $@ -L/usr/local/lib -lrtadb

main.o: main.c empd.h
	$(CC) $(CFLAGS) $< -o $@

select.o: select.c empd.h
	$(CC) $(CFLAGS) $< -o $@

logit.o: logit.c empd.h
	$(CC) $(CFLAGS) $< -o $@

ui.o: ui.c empd.h
	$(CC) $(CFLAGS) $< -o $@

config.o: config.c empd.h
	$(CC) $(CFLAGS) $< -o $@

timers.o: timers.c empd.h
	$(CC) $(CFLAGS) $< -o $@

app.o: app.c empd.h app.h
	$(CC) $(CFLAGS) $< -o $@


clean: 
	rm -rf *.o empd empd.pid

standard: clean
	for i in *.h *.c ; \
	do \
	 indent  -bad -bap -nbbb -bli0 -cdw -cli2 -cbi0 -saf -nut \
		-sai -saw -di9 -nbc -bfda -bls -lp -ci2 -i2 -nbbo -l72 \
		-fca -nbfda -nbfde -nlp -bbo -sob -cd24 -ts0 -npcs -nut \
		-T TBLDEF -T COLDEF $$i ; \
	 rm $$i~ ; \
	done

