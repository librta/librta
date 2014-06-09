/***************************************************************
 * Run Time Access
 * Copyright (C) 2003 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * app.h --
 *
 * Overview:
 *   This application demonstrates the use of the rta package
 * using select() for multiplexing.
 **************************************************************/
#include "../src/rta.h"

    /* Maximum number of UI/Posgres connections */
#define MX_UI     (20)

    /* Max size of a Postgres packet from/to the UI's */
#define MXCMD      (1000)
#define MXRSP     (50000)

    /* Note lenght and number of rows in the "mydata" table. */
#define NOTE_LEN   30
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
typedef struct
{
  int      fd;         /* FD of TCP conn (=-1 if not in use) */
  int      cmdindx;    /* Index of next location in cmd buffer */
  char     cmd[MXCMD]; /* SQL command from UI program */
  int      rspfree;    /* Number of free bytes in rsp buffer */
  char     rsp[MXRSP]; /* SQL response to the UI program */
  int      o_port;     /* Other-end TCP port number */
  int      o_ip;       /* Other-end IP address */
  long long nbytin;    /* number of bytes read in */
  long long nbytout;   /* number of bytes sent out */
  int      ctm;        /* connect time (==time();) */
  int      cdur;       /* duration time (== now()-ctm;) */
}
UI;
