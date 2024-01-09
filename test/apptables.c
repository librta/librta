/******************************************************************
 * librta library
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the MIT License.
 *  See the file COPYING file.
 *****************************************************************/

/***************************************************************
 * apptables.c --
 *
 * Overview:
 *    The "librta" package provides a Postgres-like API into our
 * system tables and variables.  We need to describe each of our
 * tables as if it were a data base table.  We describe each
 * table in general in an array of RTA_TBLDEF structures with one
 * structure per table, and each column of each table in an
 * array of RTA_COLDEF strustures with one RTA_COLDEF structure per
 * column.
 **************************************************************/

#include <stddef.h>             /* for 'offsetof' */
#include "app.h"                /* for table definitions and sizes */

extern UI ui[];
extern struct MyData mydata[];
extern int  add_demolist(char *tbl, char *sql, void *pr);
extern void del_demolist(char *tbl, char *sql, void *pr);
extern int  compute_cdur(char *tbl, char *col, char *sql, void *pr, int rowid);
extern int  reverse_str();
extern void *get_next_conn(void *prow, void *it_info, int rowid);
extern void *get_next_dlist(void *prow, void *it_info, int rowid);

/***************************************************************
 *   Here is the sample application column definitions.
 **************************************************************/
RTA_COLDEF   mycolumns[] = {
  {
      "mytable",                /* the table name */
      "myint",                  /* the column name */
      RTA_INT,                  /* it is an integer */
      sizeof(int),              /* number of bytes */
      offsetof(struct MyData, myint), /* location in struct */
      RTA_DISKSAVE,             /* save to disk */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A sample integer in a table"},
  {
      "mytable",                /* the table name */
      "myfloat",                /* the column name */
      RTA_FLOAT,                /* it is a float */
      sizeof(float),            /* number of bytes */
      offsetof(struct MyData, myfloat), /* location in struct */
      0,                        /* no flags */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A sample float in a table"},
  {
      "mytable",                /* the table name */
      "notes",                  /* the column name */
      RTA_STR,                  /* it is a string */
      NOTE_LEN,                 /* number of bytes */
      offsetof(struct MyData, notes), /* location in struct */
      RTA_DISKSAVE,             /* save to disk */
      (int (*)()) 0,            /* called before read */
      reverse_str,              /* called after write */
    "A sample note string in a table"},
  {
      "mytable",                /* the table name */
      "seton",                  /* the column name */
      RTA_STR,                  /* it is a string */
      NOTE_LEN,                 /* number of bytes */
      offsetof(struct MyData, seton), /* location in struct */
      RTA_READONLY,             /* a read-only column */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Sample of a field computed by a write callback.  Seton"
      " is the reverse of the 'notes' field."},
};

/***************************************************************
 * One of the tables we want to present to the UI is the table
 * of UI connections.  It is defined in app.h as ....
 *  typedef struct {
    int   fd;          // FD of TCP conn (=-1 if not in use)
    int   cmdindx;     // Index of next location in cmd buffer
    char  cmd[MXCMD];  // SQL command from UI program
    int   rspfree;     // Number of free bytes in rsp buffer
    char  rsp[MXRSP];  // SQL response to the UI program
    int   o_port;      // Other-end TCP port number
    int   o_ip;        // Other-end IP address 
    long long nbytin;  // number of bytes read in
    long long nbytout; // number of bytes sent out
    int   ctm;         // connect time (==time();)
    int   cdur;        // duration time (== now()-ctm;)
 *  } UI;
 * The following array of RTA_COLDEF describes this structure with
 * one RTA_COLDEF for each element in the UI strucure.
 **************************************************************/
RTA_COLDEF   ConnCols[] = {
  {
      "UIConns",                /* table name */
      "fd",                     /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, fd),         /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "File descriptor for TCP socket to UI program"},
  {
      "UIConns",                /* table name */
      "cmdindx",                /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, cmdindx),    /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Index of the next free byte in the input string, cmd"},
  {
      "UIConns",                /* table name */
      "cmd",                    /* column name */
      RTA_STR,                  /* type of data */
      MXCMD,                    /* #bytes in col data */
      offsetof(UI, cmd),        /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Input command from the user interface program.  This"
      " is an SQL command which is executed against the data"
      " in the application."},
  {
      "UIConns",                /* table name */
      "rspfree",                /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, rspfree),    /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Index of the next free byte in the response string, rsp"},
  {
      "UIConns",                /* table name */
      "rsp",                    /* column name */
      RTA_STR,                  /* type of data */
      50,                       /* first 50 bytes of response field */
      offsetof(UI, rsp),        /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Response back to the calling program.  This is used to"
      " store the result so that the write() does not need to"
      " block"},
  {
      "UIConns",                /* table name */
      "o_port",                 /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, o_port),     /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "The IP port of the other end of the connection"},
  {
      "UIConns",                /* table name */
      "o_ip",                   /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, o_ip),       /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "The IP address of the other end of the connection"
      " cast to an int."},
  {
      "UIConns",                /* table name */
      "nbytin",                 /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(long long),        /* #bytes in col data */
      offsetof(UI, nbytin),     /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Number of bytes received on this connection"},
  {
      "UIConns",                /* table name */
      "nbytout",                /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(long long),        /* #bytes in col data */
      offsetof(UI, nbytout),    /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Number of bytes sent out on this connection"},
  {
      "UIConns",                /* table name */
      "ctm",                    /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, ctm),        /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Connect TiMe.  The unix style time() when the connection"
      " was established.  This is used to decide which connection"
      " to drop when the connection table is full and a new UI"
      " connection request arrives."},
  {
      "UIConns",                /* table name */
      "cdur",                   /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, cdur),       /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      compute_cdur,             /* called before read */
      (int (*)()) 0,            /* called after write */
    "Connect DURation.  The number of seconds the connection"
      " has been open.  A read callback computes this each time"
      " the value is used."},
};

/***************************************************************
 * The table below helps illustrate how to use insert and delete
 * callbacks.  This table is also useful for testing since it has
 * one of each RTA data type.  It is implemented as a linked list.
 *  typedef struct {
    char   dlstr[NOTE_LEN];  // A string type
    void  *dlnxt;       // points to next row.  Zero at terminus.
    int    dlid;        // Unique row id == zero-indexed row #
    llong  dllong;      // A long type
    char  *dlpstr;      // A pointer to a string of NOTE_LEN bytes
    void  *dlpint;      // points to an interger
    llong  dlplong;     // points to a long type
    float  dlfloat;     // A floating point number
    float *dlpfloat;    // point to a float
    short  dlshort;     // A short integer
    uchar  dluchar;     // An unsigned character
    double dldouble;    // A double precision floating point number
 *  } DEMOLIST;
 * The following array of RTA_COLDEF describes this structure with
 * one RTA_COLDEF for each element in the DEMOLIST strucure.
 **************************************************************/
RTA_COLDEF   dlcolumns[] = {
  {
      "demotbl",                /* the table name */
      "dlstr",                  /* the column name */
      RTA_STR,                  /* it is a string */
      NOTE_LEN,                 /* number of bytes */
      offsetof(DEMOLIST, dlstr), /* location in struct */
      RTA_DISKSAVE,             /* save to disk */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A note string in a demo linked list table"},
  {
      "demotbl",                /* the table name */
      "dlnxt",                  /* the column name */
      RTA_PTR,                  /* it is a pointer */
      sizeof(void *),           /* number of bytes */
      offsetof(DEMOLIST, dlnxt), /* location in struct */
      RTA_READONLY,             /* read, don't write */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "Points to the next row in the table.  This is the link "
    "in our linked list. A zero indicated the end of the list."},
  {
      "demotbl",                /* the table name */
      "dlid",                   /* the column name */
      RTA_INT,                  /* it is an integer*/
      sizeof(int),              /* number of bytes */
      offsetof(DEMOLIST, dlid), /* location in struct */
      RTA_READONLY,             /* read, don't write */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "An integer that is the zero-indexed row number for this row."},
  {
      "demotbl",                /* the table name */
      "dllong",                 /* the column name */
      RTA_LONG,                 /* it is a long */
      sizeof(llong),            /* number of bytes */
      offsetof(DEMOLIST, dllong), /* location in struct */
      RTA_DISKSAVE,             /* Save updates to disk */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A long integer.  No meaning is assigned to this long."},
  {
      "demotbl",                /* the table name */
      "dlpstr",                 /* the column name */
      RTA_PSTR,                 /* it is a pointer to a string */
      NOTE_LEN,                 /* number of bytes */
      offsetof(DEMOLIST, dlpstr), /* location in struct */
      0,                        /* Read/write, don't save */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "This string is the reverse of the dlstr string."},
  {
      "demotbl",                /* the table name */
      "dlpint",                 /* the column name */
      RTA_PINT,                 /* it is a pointer to int */
      sizeof(int *),            /* number of bytes */
      offsetof(DEMOLIST, dlpint), /* location in struct */
      0,                        /* Read/write, don't save */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A pointer to an integer."},
  {
      "demotbl",                /* the table name */
      "dlplong",                /* the column name */
      RTA_PLONG,                /* it is a pointer to a long */
      sizeof(llong *),          /* number of bytes */
      offsetof(DEMOLIST, dlplong), /* location in struct */
      0,                        /* Read/write, don't save */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A pointer to a long."},
  {
      "demotbl",                /* the table name */
      "dlfloat",                /* the column name */
      RTA_FLOAT,                /* it is a floating point number */
      sizeof(float),            /* number of bytes */
      offsetof(DEMOLIST, dlfloat), /* location in struct */
      0,                        /* Read/write, don't save */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A floating point number."},
  {
      "demotbl",                /* the table name */
      "dlpfloat",               /* the column name */
      RTA_PFLOAT,               /* it is a pointer to a float */
      sizeof(float *),          /* number of bytes */
      offsetof(DEMOLIST, dlpfloat), /* location in struct */
      0,                        /* Read/write, don't save */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A pointer to a floating point number."},
  {
      "demotbl",                /* the table name */
      "dlshort",                /* the column name */
      RTA_SHORT,                /* it is a short integer */
      sizeof(short),            /* number of bytes */
      offsetof(DEMOLIST, dlshort), /* location in struct */
      0,                        /* Read/write, don't save */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A short integer."},
  {
      "demotbl",                /* the table name */
      "dluchar",                /* the column name */
      RTA_UCHAR,                /* it is an unsigned char */
      sizeof(unsigned char),    /* number of bytes */
      offsetof(DEMOLIST, dluchar), /* location in struct */
      0,                        /* Read/write, don't save */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "An unsigned char."},
  {
      "demotbl",                /* the table name */
      "dldouble",               /* the column name */
      RTA_DOUBLE,               /* it is a double */
      sizeof(double),           /* number of bytes */
      offsetof(DEMOLIST, dldouble), /* location in struct */
      0,                        /* Read/write, don't save */
      (int (*)()) 0,            /* called before read */
      (int (*)()) 0,            /* called after write */
    "A double precision floating point number."},
};


/***************************************************************
 *   We defined all of the data structure (column defintions)
 * for the tables above.  Now define the tables themselves.
 **************************************************************/
RTA_TBLDEF   UITables[] = {
  {
      "mytable",                /* table name */
      mydata,                   /* address of table */
      sizeof(struct MyData),    /* length of each row */
      ROW_COUNT,                /* number of rows */
      (void *) NULL,            /* iterator function */
      (void *) NULL,            /* iterator callback data */
      (void *) NULL,            /* INSERT callback */
      (void *) NULL,            /* DELETE callback */
      mycolumns,                /* array of column defs */
      sizeof(mycolumns) / sizeof(RTA_COLDEF),
      /* the number of columns */
      "/tmp/mysavefile",        /* save file name */
    "A sample application table"},
  {
      "UIConns",                /* table name */
      (void *) 0,               /* address of table */
      sizeof(UI),               /* length of each row */
      0,                        /* # rows in table */
      get_next_conn,            /* iterator function */
      (void *) NULL,            /* iterator callback data */
      (void *) NULL,            /* INSERT callback */
      (void *) NULL,            /* DELETE callback */
      ConnCols,                 /* Column definitions */
      sizeof(ConnCols) / sizeof(RTA_COLDEF), /* # columns */
      "",                       /* save file name */
    "Data about TCP connections from UI frontend programs"},
  {
      "demotbl",                /* table name */
      (void *) 0,               /* address of table */
      sizeof(DEMOLIST),         /* length of each row */
      4,                        /* number of rows */
      get_next_dlist,           /* iterator function */
      (void *) NULL,            /* iterator callback data */
      add_demolist,             /* INSERT callback */
      del_demolist,             /* DELETE callback */
      dlcolumns,                /* array of column defs */
      sizeof(dlcolumns) / sizeof(RTA_COLDEF),
      /* the number of columns */
      "/tmp/dlsavefile",        /* save file name */
    "A sample linked list table"}
};

int      nuitables = (sizeof(UITables) / sizeof(RTA_TBLDEF));

