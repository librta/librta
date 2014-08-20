/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2004 Robert W Smith (bsmith@linuxtoys.org)
 * Copyright (C) 2003-2004 Graham Phillips (graham.p@comcast.net)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * strndup.c  -- provides an implementation of strndup for those 
 * systems that don't have it. 
 **************************************************************/

#ifndef HAVE_STRNDUP

#include <stdlib.h>   /* for malloc */
#include <string.h>   /* for memcpy */
#include <errno.h>    /* for ENOMEM */


char *strndup(const char *s, size_t n)
{
   char   *dest; 
   size_t  i  = 0;
   char   *p = (char*)s; 
   while (i<n && *p++)
      i++; 
   n = i;
   dest = (char*)malloc(n + 1);
   if (dest == NULL) {
      errno = ENOMEM; 
      return NULL;
   }
   memcpy(dest,s,n);
   dest[n] = 0;
   return dest;
}

#endif
