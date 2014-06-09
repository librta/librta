
/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2004 Robert W Smith (bsmith@linuxtoys.org)
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

    /* types of relations allowed in WHERE */
#define RTA_EQ        0
#define RTA_NE        1
#define RTA_GT        2
#define RTA_LT        3
#define RTA_GE        4
#define RTA_LE        5

    /* SQL errors to the front ends */
#define E_NOTABLE  "Relation '%s' does not exist"
#define E_NOCOLUMN  "Attribute '%s' not found"
#define E_BADPARSE  "SQL parse error",""
#define E_BIGSTR    "String too long for '%s'"
#define E_NOWRITE   "Can not update read-only column '%s'"
#define E_FULLBUF   "Output buffer full",""

    /* Defines for the meta tables.  The table of tables must always be 
       table #0, and the table of columns must always be table #1. */
#define RTA_TABLES    0
#define RTA_COLUMNS   1

    /* Used to remove a few characters from dbg() lines */
#define LOC __FILE__,__LINE__

    /* Maximum number of characters in printed data type */
#define MX_INT_STRING    (12)
#define MX_LONG_STRING   (24)
#define MX_FLOT_STRING   (24)

    /* Max # strings in our private stack for yacc */
#define MXPARSESTR   ((NCMDCOLS *2) + 4)

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
  char    *sqlcmd;     /* points to text of SQL command */
  int      command;    /* RTA_SELECT or UPDATE */
  char    *tbl;        /* the table in question */
  TBLDEF  *ptbl;       /* pointer to table in TBLDEFS */
  int      itbl;       /* Index of table in Tbl */
  int      ncols;      /* count of columns to display/update */
  char    *cols[NCMDCOLS]; /* col to display/update */
  COLDEF  *pcol[NCMDCOLS]; /* pointers to cols in COLDEFS */
  char    *updvals[NCMDCOLS]; /* values for column updates */
  int      updints[NCMDCOLS]; /* integer values for updates */
  long long updlngs[NCMDCOLS]; /* long values for updates */
  float    updflot[NCMDCOLS]; /* float values for updates */
  int      nwhrcols;   /* count of columns in where clause */
  char    *whrcols[NCMDCOLS]; /* cols in where */
  int      whrrel[NCMDCOLS]; /* relation (EQ, GT...) in where */
  COLDEF  *pwhr[NCMDCOLS]; /* pointers to Wcols in COLDEFS */
  char    *whrvals[NCMDCOLS]; /* values in the where clause */
  int      whrints[NCMDCOLS]; /* integer values of whrvals[] */
  long long whrlngs[NCMDCOLS]; /* long values of whrvals[] */
  float    whrflot[NCMDCOLS]; /* float values of whrvals[] */
  int      limit;      /* max num rows to output, 0=no_limit */
  int      offset;     /* scan past this # rows before output */
  char    *out;        /* put command response here */
  int     *nout;       /* I/O number free bytes at 'out' */
  char    *errout;     /* ==out at start. But for err msgs */
  int      nerrout;    /* ==nout at start. But for err msgs */
  int      err;        /* set =1 if error in SQL parse */
  int      nlineout;   /* #bytes in SELECT row response */
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
  char     ident[MXDBGIDENT]; /* ident string for syslog() */
};

/* Define the stats structure */
struct RtaStat
{
  long long nsyserr;   /* count of failed OS calls. */
  long long nrtaerr;   /* count of internal rta failures. */
  long long nsqlerr;   /* count of SQL failures. */
  long long nauth;     /* count of DB authorizations. */
  long long nupdate;   /* count of UPDATE requests */
  long long nselect;   /* count of SELECT requests */
};

/* Forward references */
void     do_sql(char *, int *);
void     send_error(char *, int, char *, char *);
void     verify_table_name(char *, int *);
void     verify_select_list(char *, int *);
void     verify_update_list(char *, int *);
void     verify_where_list(char *, int *);
void     do_update(char *, int *);
void     do_update(char *, int *);
int      send_row_description(char *, int *);
void     do_select(char *, int *);
void     do_call(char *, int *);
void     ad_str(char **, char *);
void     ad_int2(char **, int);
void     ad_int4(char **, int);
void     rtalog(char *, int, char *, ...);

#endif
