
/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2004 Robert W Smith (bsmith@linuxtoys.org)
 * Copyright (C) 2003-2004 Graham Phillips (graham.p@comcast.net)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * dirname.c  -- provides an implementation of dirname for those 
 * systems that don't have it. 
 **************************************************************/

#ifndef HAVE_DIRNAME

#include <string.h>   /* for strlen */

/**
 * dirname breaks a null-terminated pathname string into directory and
 * filename components.  In the usual case, dirname returns the string
 * up to, but not including, the final '/'.  Trailing '/' characters are not
 * counted as part of the pathname.
 *
 * If path does not contain a slash, dirname returns the string ".".  If
 * path is the string "/", then dirame returns the string "/".  If path is a
 * NULL pointer or points to an empty string, then dirname returns the string
 * ".".
 * dirnme may modify the contents of path, so if you need to preserve the
 * pathname string, copies should be passed to these functions.  Furthermore,
 * dirame may return pointers to statically allocated memory which may
 * overwritten by subsequent calls.
 */


char *dirname(char *path)
{ 
    static char period[2] = ".";
    char *ptr;
    int len; 

    if (!path)
       return period;

    len = strlen(path);
    if (len <= 0)
       return period;

    ptr = path + len-1;
    while (ptr > path && *ptr == '/')
        ptr--; 
    while (ptr > path && *ptr != '/') 
        ptr--;
    if (ptr == path && *ptr == '/') {
        *(ptr+1) = '\0';
        return path;
    }
    if (ptr == path && *ptr != '/')
        return period;

    /* At this point, ptr > path && *ptr == '/' */
    *ptr = '\0';
    return path;
}

#endif


#ifdef TEST_DIRNAME

int main() 
{
    char path[100];
    char *r;
    strcpy(path,"/usr/lib");
    printf("%s ", path); 
    r = dirname(path); 
    printf("%s\n", r); 

    strcpy(path,"/usr/");
    printf("%s ", path); 
    r = dirname(path); 
    printf("%s\n", r); 

    strcpy(path,"/");
    printf("%s ", path); 
    r = dirname(path); 
    printf("%s\n", r); 

    strcpy(path,"usr");
    printf("%s ", path); 
    r = dirname(path); 
    printf("%s\n", r); 

    strcpy(path,".");
    printf("%s ", path); 
    r = dirname(path); 
    printf("%s\n", r); 

    strcpy(path,"..");
    printf("%s ", path); 
    r = dirname(path); 
    printf("%s\n", r); 

    strcpy(path,"aaaaa/bbbbb/////");
    printf("%s ", path); 
    r = dirname(path); 
    printf("%s\n", r); 
}

#endif
