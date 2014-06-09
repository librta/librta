/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2006 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * do_sql.h
 **************************************************************/

#ifndef DO_SQL_H
#define DO_SQL_H 1

#include "rta.h"

    /* types of SQL statements recognized */
#define RTA_SELECT    0
#define RTA_UPDATE    1
#define RTA_INSERT    2
#define RTA_DELETE    3

    /* types of relations allowed in WHERE */
#define RTA_EQ        0
#define RTA_NE        1
#define RTA_GT        2
#define RTA_LT        3
#define RTA_GE        4
#define RTA_LE        5

    /* Defines for the meta tables.  The table of tables must always be 
       table #0, and the table of columns must always be table #1. */
#define RTA_TABLES    ((void *) 0)
#define RTA_COLUMNS   ((void *) 1)

    /* Used to remove a few characters from dbg() lines */
#define LOC __FILE__,__LINE__

    /* Maximum number of characters in printed data type */
#define MX_INT_STRING    (12)
#define MX_LONG_STRING   (24)
#define MX_FLOT_STRING   (24)

    /* Max # strings in our private stack for yacc */
#define MXPARSESTR   ((RTA_NCMDCOLS *2) + 4)

/** ************************************************************
 * This structure contains/encodes the parsed SQL command from
 * one of the UI or client interfaces.
 * This structure is filled in by the yacc parser.  If the parse
 * is successful, the completed structure is passed to do_sql()
 * for execution.
 *
 * The 'command' is just the type of SQL command.
 * The 'cols' field is a list of names from the "SELECT cols"
 * or from the "UPDATE col=X [,...]".
 * The 'vals' field is a list of the strings "X" in an UPDATE.
 * The 'tbl' field has the name of the table in use.
 * The 'whrcols' and 'whrvals' fields are similar to the cols
 * and vals fields.  
 * Note that cols, vals, whrcols, and whrvals in the structure
 * below point to alloc()'ed memory and must be freed when done.
 **************************************************************/
struct Sql_Cmd
{
  char        *sqlcmd;     /* points to text of SQL command */
  int          command;    /* RTA_SELECT or UPDATE */
  char        *tbl;        /* the table in question */
  RTA_TBLDEF  *ptbl;       /* pointer to table in TBLDEFS */
  int          itbl;       /* Index of table in Tbl */
  int          ncols;      /* count of columns to display/update */
  char        *cols[RTA_NCMDCOLS]; /* col to display/update */
  RTA_COLDEF  *pcol[RTA_NCMDCOLS]; /* pointers to cols in COLDEFS */
  char        *updvals[RTA_NCMDCOLS]; /* values for column updates */
  int          updints[RTA_NCMDCOLS]; /* integer values for updates */
  llong        updlngs[RTA_NCMDCOLS]; /* long values for updates */
  float        updflot[RTA_NCMDCOLS]; /* float values for updates */
  double       upddbl[RTA_NCMDCOLS];  /* double values for updates */
  int          nwhrcols;   /* count of columns in where clause */
  char        *whrcols[RTA_NCMDCOLS]; /* cols in where */
  int          whrrel[RTA_NCMDCOLS];  /* relation (EQ, GT...) in where */
  RTA_COLDEF  *pwhr[RTA_NCMDCOLS];    /* pointers to Wcols in COLDEFS */
  char        *whrvals[RTA_NCMDCOLS]; /* values in the where clause */
  int          whrints[RTA_NCMDCOLS]; /* integer values of whrvals[] */
  llong        whrlngs[RTA_NCMDCOLS]; /* long values of whrvals[] */
  float        whrflot[RTA_NCMDCOLS]; /* float values of whrvals[] */
  double       whrdbl[RTA_NCMDCOLS];  /* double values of whrvals[] */
  int          limit;      /* max num rows to output, 0=no_limit */
  int          offset;     /* scan past this # rows before output */
  char        *out;        /* put command response here */
  int         *nout;       /* I/O number free bytes at 'out' */
  char        *errout;     /* ==out at start. But for err msgs */
  int          nerrout;    /* ==nout at start. But for err msgs */
  int          err;        /* set =1 if error in SQL parse */
  int          nlineout;   /* #bytes in SELECT row response */
};

/* Define the debug config structure */
struct RtaDbg
{
  int      syserr;     /* !=0 to log system errors */
  int      rtaerr;     /* !=0 to log rta errors */
  int      sqlerr;     /* !=0 to log SQL errors */
  int      trace;      /* !=0 to log SQL commands */
  int      target;     /* 0=off, 1=syslog, 2=stderr, 3=both */
  int      priority;   /* syslog() priority level */
  int      facility;   /* syslog() facility */
  char     ident[RTA_MXDBGIDENT]; /* ident string for syslog() */
};

/* Define the stats structure */
struct RtaStat
{
  llong nsyserr;       /* count of failed OS calls. */
  llong nrtaerr;       /* count of internal rta failures. */
  llong nsqlerr;       /* count of SQL failures. */
  llong nauth;         /* count of DB authorizations. */
  llong nupdate;       /* count of UPDATE requests */
  llong nselect;       /* count of SELECT requests */
  llong ninsert;       /* count of INSERT requests */
  llong ndelete;       /* count of DELETE requests */
};

/* Forward references */
void     rta_do_sql(char *, int *);
void     rta_send_error(char *, int, char *, char *);
void     rta_log(char *, int, char *, ...);

#endif
