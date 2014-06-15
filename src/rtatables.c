/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * Overview:
 *    The "rta" package provides a Postgres-like API into our
 * system tables and variables.  We need to describe each of our
 * tables as if it were a data base table.  We describe each
 * table in general in an array of RTA_TBLDEF structures with one
 * structure per table, and each column of each table in an
 * array of RTA_COLDEF structures with one RTA_COLDEF structure per
 * column.
 **************************************************************/

#include <stdio.h>              /* for printf prototypes */
#include <stddef.h>             /* for 'offsetof' */
#include <syslog.h>             /* for LOG_ERR, LOG_USER */
#include <string.h>             /* for strncmp prototypes */
#include "rta.h"                /* for RTA_TBLDEF and RTA_COLDEF */
#include "do_sql.h"             /* for struct Sql_Cmd */

/* Forward reference for read callbacks and iterators */
int          rta_restart_syslog();
static void *get_next_sysrow(void *, void *, int);

/***************************************************************
 * We define a table which contains the column definitions of
 * all columns in the system.  This is a pseudo table in that
 * there is not an array of structures like other tables.
 * Instead, pointers to each column definition is placed in a
 * table.  This table requires special handling in do_sql.c.
 * The table definition for "rta_columns" must appear as the
 * second entry in the array of table definition pointers.
 **************************************************************/

/* Define the table columns */
RTA_COLDEF   rta_columnsCols[] = {
  {
      "rta_columns",            /* table name */
      "table",                  /* column name */
      RTA_PSTR,                 /* type of data */
      RTA_MXTBLNAME,            /* #bytes in col data */
      offsetof(RTA_COLDEF, table), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The name of the table that this column belongs to."},
  {
      "rta_columns",            /* table name */
      "name",                   /* column name */
      RTA_PSTR,                 /* type of data */
      RTA_MXCOLNAME,            /* #bytes in col data */
      offsetof(RTA_COLDEF, name), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The name of the column.  Must be unique within a table "
      "definition but may be replicated in other tables.  The "
      "maximum string length of the column name is set by "
      "RTA_MXCOLNAME defined in the rta.h file."},
  {
      "rta_columns",            /* table name */
      "type",                   /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(RTA_COLDEF, type), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The data type of the column.  Types include string, "
      "integer, long, pointer, pointer to string, pointer to "
      "integer, and pointer to long.  See rta.h for more details."},
  {
      "rta_columns",            /* table name */
      "length",                 /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(RTA_COLDEF, length), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The length of the string in bytes if the column data "
      "type is a string or a pointer to a string."},
  {
      "rta_columns",            /* table name */
      "noff",                   /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_COLDEF, offset), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The number of bytes from the start of the structure "
      "to the member element defined in this entry.  Be careful "
      "in setting the offset with non word-aligned elements like "
      "single characters.  If you do no use offsetof() consider "
      "using -fpack-struct.  (By the way, the column name is "
      "actually 'offset' but that conflicts with one of the SQL words"},
  {
      "rta_columns",            /* table name */
      "flags",                  /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(RTA_COLDEF, flags), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Flags associated with the column include flags to indicate "
      "read-only status and whether or not the data should be "
      "included in the save file.  See rta.h for the associated "
      "defines and details."},
  {
      "rta_columns",            /* table name */
      "readcb",                 /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_COLDEF, readcb), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A pointer to a function that returns an integer.  If "
      "defined, the function is called before the column is "
      "read.  This function is useful to compute values only "
      "when needed.  A zero is returned by the callback if the "
      "callback succeeds."},
  {
      "rta_columns",            /* table name */
      "writecb",                /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_COLDEF, writecb), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A pointer to a function that returns and integer.  If "
      "defined, the function is called after an UPDATE command "
      "modifies the column.  All columns in an UPDATE are "
      "modified before any write callbacks are executed.  This "
      "function is useful to effect changes requested or implied "
      "by the column definition. The function return a zero on "
      "success.  If a non-zero value is returned, the SQL client "
      "receives an TRIGGERED ACTION EXCEPTION error."},
  {
      "rta_columns",            /* table name */
      "help",                   /* column name */
      RTA_PSTR,                 /* type of data */
      RTA_MXHELPSTR,            /* #bytes in col data */
      offsetof(RTA_COLDEF, help), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A brief description of the column.  Should include "
      "limits, default value, and a description of how to set "
      "it.  Can contain at most RTA_MXHELPSTR characters."},
};

/* Define the table */
RTA_TBLDEF   rta_columnsTable = {
  "rta_columns",                /* table name */
  (void *) 0,                   /* address of table */
  sizeof(RTA_COLDEF),               /* length of each row */
  0,                            /* incremented as tables are added */
  get_next_sysrow,              /* iterator function */
  (void *) RTA_COLUMNS,         /* iterator callback data */
  (void *) NULL,                /* INSERT callback function */
  (void *) NULL,                /* DELETE callback function */
  rta_columnsCols,              /* Column definitions */
  sizeof(rta_columnsCols) / sizeof(RTA_COLDEF), /* # columns */
  "",                           /* save file name */
  "The list of all columns in all tables along with their "
    "attributes."
};

/***************************************************************
 * We define a table which contains the table definition of all
 * tables in the system.  This is a pseudo table in that there
 * is not an array of structures like other tables.  Use of this
 * table requires special handling in do_sql.
 **************************************************************/

/* Define the table columns */
RTA_COLDEF   rta_tablesCols[] = {
  {
      "rta_tables",             /* table name */
      "name",                   /* column name */
      RTA_PSTR,                 /* type of data */
      RTA_MXTBLNAME,            /* #bytes in col data */
      offsetof(RTA_TBLDEF, name), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The name of the table.  This must be unique in the system. "
      " Table names can be at most RTA_MXTBLNAME characters in length."
      "  See rta.h for details.  Note that some table names are "
      "reserved for internal use."},
  {
      "rta_tables",             /* table name */
      "address",                /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_TBLDEF, address), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The start address of the array of structs that makes up "
      "the table."},
  {
      "rta_tables",             /* table name */
      "rowlen",                 /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(RTA_TBLDEF, rowlen), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The length of each struct in the array of structs that "
      "makes up the table."},
  {
      "rta_tables",             /* table name */
      "nrows",                  /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(RTA_TBLDEF, nrows), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The number of rows in the table."},
  {
      "rta_tables",             /* table name */
      "iterator",               /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_TBLDEF, iterator), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The iterator is a function that, given a pointer to a "
      "row, returns a pointer to the next row.  When passed "
      "a NULL as input, the function returns a pointer to the "
      "first row of the table.  The function return a NULL when "
      "asked for the row after the last row.  The function is "
      "useful to walk through the rows of a linked list."},
  {
      "rta_tables",             /* table name */
      "it_info",                /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_TBLDEF, it_info), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "This is a pointer to any kind of information that the "
      "caller wants returned with each iterator call.  For "
      "example, you may wish to have one iterator function "
      "for all of your linked lists.  You could pass in "
      "a unique identifier for each table so the function "
      "can handle each one as appropriate. "},
  {
      "rta_tables",             /* table name */
      "insertcb",               /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_TBLDEF, insertcb), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The INSERT callback is invoked to add a row to a table. "
      "It is used mostly with tables in which the rows are "
      "dynamically allocated. "},
  {
      "rta_tables",             /* table name */
      "deletecb",               /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_TBLDEF, deletecb), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The DELETE callback is invoked to remove a row from a "
      "table.  It is used mostly with tables in which the rows "
      "are dynamically allocated. "},
  {
      "rta_tables",             /* table name */
      "cols",                   /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(RTA_TBLDEF, cols), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A pointer to an array of RTA_COLDEF structures.  There is one "
      "RTA_COLDEF for each column in the table."},
  {
      "rta_tables",             /* table name */
      "ncol",                   /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(RTA_TBLDEF, ncol), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The number of columns in the table."},
  {
      "rta_tables",             /* table name */
      "savefile",               /* column name */
      RTA_PSTR,                 /* type of data */
      RTA_MXFILENAME,           /* #bytes in col data */
      offsetof(RTA_TBLDEF, savefile), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The name of the file with the non-volatile contents of "
      "the table.  This file is read when the table is "
      "initialized and is written any time a column with the "
      "non-volatile flag set is modified."},
  {
      "rta_tables",             /* table name */
      "help",                   /* column name */
      RTA_PSTR,                 /* type of data */
      RTA_MXHELPSTR,            /* #bytes in col data */
      offsetof(RTA_TBLDEF, help), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A description of the table."},
};

/* Define the table */
RTA_TBLDEF   rta_tablesTable = {
  "rta_tables",                 /* table name */
  (void *) 0,                   /* address of table */
  sizeof(RTA_TBLDEF),               /* length of each row */
  0,                            /* It's a pseudo table */
  get_next_sysrow,              /* iterator function */
  (void *) RTA_TABLES,          /* iterator callback data */
  (void *) NULL,                /* INSERT callback function */
  (void *) NULL,                /* DELETE callback function */
  rta_tablesCols,               /* Column definitions */
  sizeof(rta_tablesCols) / sizeof(RTA_COLDEF), /* # columns */
  "",                           /* save file name */
  "The table of all tables in the system.  This is a pseudo "
    "table and not an array of structures like other tables."
};

/***************************************************************
 * get_next_sysrow(): - Routine to the get the next row pointer
 * given the current row pointer or row number.
 *
 * Input:        Pointer to current row
 *               Callback data (RTA_TABLES or RTA_COLUMNS)
 *               Desired row number (ie current +1)
 * Output:       Pointer to next row or NULL if at end of list
 * Effects:      None
 **************************************************************/
static void    *
get_next_sysrow(void *pui, void *it_info, int rowid)
{
  extern RTA_TBLDEF *rta_Tbl[];
  extern RTA_COLDEF *rta_Col[];
  extern int rta_Ntbl;
  extern int rta_Ncol;

  /* The tables for tables and columns contain pointers to the actual
     RTA_TBLDEF and RTA_COLDEF structures.  This saves memory and makes it easy 
     for a program to change parts of the table or column definition
     when needed.  So this routine just return the rta_Tbl or rta_Col value. */
  if ((it_info == RTA_TABLES) && (rowid < rta_Ntbl)) {
    return ((void *) rta_Tbl[rowid]);
  }
  else if ((it_info == RTA_COLUMNS) && (rowid < rta_Ncol)) {
    return ((void *) rta_Col[rowid]);
  }

  /* Must be at end of list */
  return ((void *) NULL);
}

/***************************************************************
 *     The rta_dbg table controls which errors generate
 * debug log messages, the priority, and the facility of the
 * syslog() messages sent.  The rta package generates no user
 * level log * messages, only debug messages.  All of the fields
 * in this table are volatile.  You will need to set the values
 * in your main program to make them seem persistent.
 * (Try something like
 *    "rta_SQL_string("UPDATE rta_dbgconfig SET dbg ....").)
 * A callback attached to dbg_facility causes a close/reopen of
 * syslog().
 **************************************************************/

/* Allocate and initialize the table */
struct RtaDbg rta_dbg = {
  1,                            /* log system errors */
  1,                            /* log rta errors */
  1,                            /* log SQL errors */
  0,                            /* no log of SQL cmds */
  1,                            /* log to syslog() only */
  LOG_ERR,                      /* see sys/syslog.h */
  LOG_USER,                     /* see sys/syslog.h */
  "rta"                         /* see 'man openlog' */
};

/* Define the table columns */
RTA_COLDEF   rta_dbgCols[] = {
  {
      "rta_dbg",                /* table name */
      "syserr",                 /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(struct RtaDbg, syserr), /* offset 2 col strt */
      0,               /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A non-zero value causes a call to syslog() for all system "
      "errors such as failed malloc() or save file read failures."},
  {
      "rta_dbg",                /* table name */
      "rtaerr",                 /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(struct RtaDbg, rtaerr), /* offset 2 col strt */
      0,               /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A non-zero value causes a call to syslog() for all errors "
      "internal to the rta package."},
  {
      "rta_dbg",                /* table name */
      "sqlerr",                 /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(struct RtaDbg, sqlerr), /* offset 2 col strt */
      0,               /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A non-zero value causes a call to syslog() for all SQL "
      "errors.  Such errors usually indicate a programming error "
      "in one of the user interface programs."},
  {
      "rta_dbg",                /* table name */
      "trace",                  /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(struct RtaDbg, trace), /* offset 2 col strt */
      0,               /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "A non-zero value causes all SQL commands to be logged.  "
      "If the command is UPDATE, the number of rows affected is "
      "also logged."},
  {
      "rta_dbg",                /* table name */
      "target",                 /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(struct RtaDbg, target), /* offset 2 col strt */
      0,               /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      rta_restart_syslog,  /* called after write */
      "Sets destination of log messages.  Zero turns off all "
      "logging of errors.  One sends log messages to syslog()."
      "  Two sends log messages to stderr.  Three sends error "
      "messages to both syslog() and to stderr.  Default is one."},
  {
      "rta_dbg",                /* table name */
      "priority",               /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(struct RtaDbg, priority), /* offset 2 col strt */
      0,               /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The syslog() priority.  Please see .../sys/syslog.h for "
      "the possible values.  Default is LOG_ERR."},
  {
      "rta_dbg",                /* table name */
      "facility",               /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(struct RtaDbg, facility), /* offset 2 col strt */
      0,               /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The syslog() facility.  Please see .../sys/syslog.h for "
      "the possible values.  Default is LOG_USER."},
  {
      "rta_dbg",                /* table name */
      "ident",                  /* column name */
      RTA_STR,                  /* type of data */
      RTA_MXDBGIDENT,           /* #bytes in col data */
      offsetof(struct RtaDbg, ident), /* offset 2 col strt */
      0,               /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "The syslog() 'ident'.  Please see 'man openlog' for "
      "details.  Default is 'rta'.  An update of the target "
      "field is required for this to take effect."},
};

/***************************************************************
 * rta_restart_syslog(): - Routine to restart or reconfigure the
 * logging facility.  Syslog is always closed and (if enabled)
 * reopened with the priority and facility specified in the
 * rta_dbg structure.
 *
 * Input:        Name of the table 
 *               Name of the column
 *               Text of the SQL command itself
 *               Pointer to row of data
 *               Pointer to copy of old row
 *               Index of row used (zero indexed)
 * Output:       Success (a zero)
 * Effects:      No side effects.
 **************************************************************/
int
rta_restart_syslog(char *tblname, char *colname, char *sqlcmd,
               void *prow, void *poldrow,  int rowid)
{
  extern struct RtaDbg rta_dbg;

  closelog();

  if (rta_dbg.target == 1 || rta_dbg.target == 3) {
    openlog(rta_dbg.ident, LOG_ODELAY | LOG_PID, rta_dbg.facility);
  }

  return(0);
}

/* Define the table */
RTA_TBLDEF   rta_dbgTable = {
  "rta_dbg",                    /* table name */
  (void *) &rta_dbg,             /* address of table */
  sizeof(struct RtaDbg),        /* length of each row */
  1,                            /* # rows in table */
  (void *) NULL,                /* iterator function */
  (void *) NULL,                /* iterator callback data */
  (void *) NULL,                /* INSERT callback function */
  (void *) NULL,                /* DELETE callback function */
  rta_dbgCols,                  /* Column definitions */
  sizeof(rta_dbgCols) / sizeof(RTA_COLDEF), /* # columns */
  "",                           /* save file name */
  "Configure of debug logging.  A callback on the 'target' "
    "field closes and reopens syslog().  None of the values "
    "in this table are saved to disk.  If you want non-default "
    "values you need to change the rta source or do an "
    "rta_SQL_string() to set the values when you initialize your "
    "program."
};

/***************************************************************
 *     The rta_stats table contains usage and error statistics
 * which might be of interest to developers.  All fields are
 * of type long, are read-only, and are set to zero by
 * rta_init(). 
 **************************************************************/

/* Allocate and initialize the table */
struct RtaStat rta_stat = {
  (llong) 0,                    /* count of failed OS calls. */
  (llong) 0,                    /* count of internal rta failures. */
  (llong) 0,                    /* count of SQL failures. */
  (llong) 0,                    /* count of authorizations. */
  (llong) 0,                    /* count of UPDATE requests */
  (llong) 0,                    /* count of SELECT requests */
};

/* Define the table columns */
RTA_COLDEF   rta_statCols[] = {
  {
      "rta_stat",               /* table name */
      "nsyserr",                /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(llong),            /* #bytes in col data */
      offsetof(struct RtaStat, nsyserr), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Count of failed OS calls."},
  {
      "rta_stat",               /* table name */
      "nrtaerr",                /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(llong),            /* #bytes in col data */
      offsetof(struct RtaStat, nrtaerr), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Count of internal rta failures."},
  {
      "rta_stat",               /* table name */
      "nsqlerr",                /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(llong),            /* #bytes in col data */
      offsetof(struct RtaStat, nsqlerr), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Count of SQL failures."},
  {
      "rta_stat",               /* table name */
      "nauth",                  /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(llong),            /* #bytes in col data */
      offsetof(struct RtaStat, nauth), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Count of DB authorizations.  This is a good estimate "
      "to the total number of connections."},
  {
      "rta_stat",               /* table name */
      "nselect",                /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(llong),            /* #bytes in col data */
      offsetof(struct RtaStat, nselect), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Count of SELECT commands."},
  {
      "rta_stat",               /* table name */
      "nupdate",                /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(llong),            /* #bytes in col data */
      offsetof(struct RtaStat, nupdate), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Count of UPDATE commands."},
  {
      "rta_stat",               /* table name */
      "ninsert",                /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(llong),            /* #bytes in col data */
      offsetof(struct RtaStat, ninsert), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Count of INSERT commands."},
  {
      "rta_stat",               /* table name */
      "ndelete",                /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(llong),            /* #bytes in col data */
      offsetof(struct RtaStat, ndelete), /* offset 2 col strt */
      RTA_READONLY,    /* Flags for read-only/disksave */
      (int (*)()) 0,  /* called before read */
      (int (*)()) 0,  /* called after write */
      "Count of DELETE commands."},
};

/* Define the table */
RTA_TBLDEF   rta_statTable = {
  "rta_stat",                   /* table name */
  (void *) &rta_stat,           /* address of table */
  sizeof(struct RtaStat),       /* length of each row */
  1,                            /* # rows in table */
  (void *) NULL,                /* iterator function */
  (void *) NULL,                /* iterator callback data */
  (void *) NULL,                /* INSERT callback function */
  (void *) NULL,                /* DELETE callback function */
  rta_statCols,                 /* Column definitions */
  sizeof(rta_statCols) / sizeof(RTA_COLDEF), /* # columns */
  "",                           /* save file name */
  "Usage and error counts for the rta package."
};
