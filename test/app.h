/******************************************************************
 * librta library
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the MIT License.
 *  See the file COPYING file.
 *****************************************************************/

/***************************************************************
 * app.h --
 *
 * Overview:
 *   This application demonstrates the use of the librta package
 * using select() for multiplexing.
 **************************************************************/
#include "librta.h"

    /* Maximum number of UI/Posgres connections */
#define MX_UI     (20)

    /* Max size of a Postgres packet from/to the UI's */
#define MXCMD      (1000)
#define MXRSP     (50000)

    /* Note length and number of rows in the "mydata" table. */
#define NOTE_LEN   20
#define ROW_COUNT  20

/*  -structure definitions */
/***************************************************************
 * the structure for the sample application table.  This is 
 * where you would put your real application tables.
 **************************************************************/
struct MyData
{
  int      myint;
  float    myfloat;
  char     notes[NOTE_LEN];
  char     seton[NOTE_LEN];
};

/***************************************************************
 * the information kept for TCP connections to UI programs
 **************************************************************/
typedef struct ui
{
  struct ui  *prevconn;   /* Points to previous conn in linked list */
  struct ui  *nextconn;   /* Points to next conn in linked list */
  int      fd;         /* FD of TCP conn (=-1 if not in use) */
  int      cmdindx;    /* Index of next location in cmd buffer */
  char     cmd[MXCMD]; /* SQL command from UI program */
  int      rspfree;    /* Number of free bytes in rsp buffer */
  char     rsp[MXRSP]; /* SQL response to the UI program */
  int      o_port;     /* Other-end TCP port number */
  int      o_ip;       /* Other-end IP address */
  llong    nbytin;     /* number of bytes read in */
  llong    nbytout;    /* number of bytes sent out */
  int      ctm;        /* connect time (==time();) */
  int      cdur;       /* duration time (== now()-ctm;) */
} UI;


/***************************************************************
 * A demo linked list table
 **************************************************************/
typedef struct {
    char   dlstr[NOTE_LEN];  // A string type
    void  *dlnxt;       // points to next row.  Zero at terminus.
    int    dlid;        // Unique row id == zero-indexed row #
    llong  dllong;      // A long type
    char  *dlpstr;      // A pointer to a string of NOTE_LEN bytes
    void  *dlpint;      // points to an interger
    llong *dlplong;     // points to a long type
    float  dlfloat;     // A floating point number
    float *dlpfloat;    // point to a float
    short  dlshort;     // A short integer
    unsigned char dluchar;     // An unsigned character
    double dldouble;    // A double precision floating point number
} DEMOLIST;
