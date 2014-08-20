/*
 * mkstemp(): create a unique temporary file
 * written by Cornelius Krasel - (c) 2000
 * bugfixes by Matthias Andree - (c) 2002
 *
 * Matthias Andree says: As far as I know, not being a lawyer, the license (for
 * this file) is compatible with the GNU GPL and GNU LGPL. The "old" leafnode
 * license that was used for that stuff is called the "MIT/X11 public license"
 * by many.
 */

/***************************************************************
 * mkstemp.c  -- provides an implementation of mkstemp for those 
 * systems that don't have it. 
 **************************************************************/

#ifndef HAVE_MKSTEMP

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#include <stdlib.h>
#define ATTEMPTS 5

int
mkstemp(char *template)
{
    int i, j, fd;
    char *c;
    char use[] =		/* 62 chars to choose from */
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    c = (template + strlen(template) - 1);
    i = 0;
    while ((*c-- == 'X') && (*c != *template))
	i++;
    if (i < 6)
	/* less than 6 X's */
	return EINVAL;
    i = 0;
    while (i < ATTEMPTS) {
	srand((unsigned int)(time(NULL) + i));
	/* initialize random number generator */
	for (j = 0; j < 6; j++) {
	    /* generate 0<x<61 from the random number and use it as index */
	    *(c + j + 1) = use[(int)(62.0 * rand() / (RAND_MAX + 1.0))];
	}
	fd = open(template, O_RDWR | O_EXCL | O_CREAT, 0600);
	if (fd >= 0)
	    return fd;		/* success */
	i++;
    }
    return EEXIST;		/* we failed */
}
#endif				/* HAVE_MKSTEMP */
/* ANSI C forbids an empty source file... */
static void
dummy_func(void)
{
    dummy_func();
}
