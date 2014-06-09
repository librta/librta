
/***************************************************************
 * Overview:
 *    The "rta" package provides a Postgres-like API into our
 * system tables and variables.  We need to describe each of our
 * tables as if it were a data base table.  We describe each
 * table in general in an array of TBLDEF structures with one
 * structure per table, and each column of each table in an
 * array of COLDEF strustures with one COLDEF structure per
 * column.
 **************************************************************/

#include <stdio.h>              /* for printf prototypes */
#include <stddef.h>             /* for 'offsetof' */
#include <syslog.h>             /* for LOG_ERR, LOG_USER */
#include <string.h>             /* for strncmp prototypes */
#include "rta.h"                /* for TBLDEF and COLDEF */
#include "do_sql.h"             /* for struct Sql_Cmd */

/* Forward reference for read callbacks */
void  spoof_user(char *,
                 char *,
                 char *,
                 int);
void  restart_syslog(char *,
                     char *,
                     char *,
                     int);

/***************************************************************
 * One of the tables we need to maintain compatibility with the
 * various Postgres front-ends is 'pg_user'.  So here we define
 * a pg_user data structure, allocate a 1 row table for users,
 * define the table using COLDEF and TBLDEF, and give a read
 * callback which makes the first table entry look like whoever
 * appears in the WHERE clause.  So a query like ....
 *    SELECT usesuper FROM pg_user WHERE usename = "Any Name"
 * will always return 'f' regardless of the usename requested.
 * This is also a resonable demonstration of a read callback.
 * (Clearly this defeats the security model of Postgres.  If you
 * _want_ to use a real pg_user table you can remove the read
 * callback, allocate more rows, and save the data to disk.)
 **************************************************************/

#define   MX_PGNAMELEN   (32)
#define   FT_LEN          (2)

/* Define the structure */
struct Pg_User
{
  char  usename[MX_PGNAMELEN]; /* user name */
  int   usesysid;      /* user ID */
  char  usecreatedb[FT_LEN]; /* creat DB */
  char  usetrace[FT_LEN]; /* trace execution */
  char  usesuper[FT_LEN]; /* super user */
  char  usecatupd[FT_LEN];
  char  passwd[MX_PGNAMELEN]; /* the password */
  char  valuntil[MX_PGNAMELEN]; /* valid until .... */
};

/* Allocate and initialize the table */
struct Pg_User pg_user[] = {
  {
   "Any Name Here",             /* user name */
   100,                         /* user ID */
   "f",                         /* creat DB */
   "f",                         /* trace execution */
   "f",                         /* super user */
   "f",
   "********",                  /* the password */
   ""                           /* valid until .... */
   }
};

/* Define the table columns */
COLDEF pg_userCols[] = {
  {
   "pg_user",                   /* table name */
   "usename",                   /* column name */
   RTA_STR,                     /* type of data */
   MX_PGNAMELEN,                /* #bytes in col data */
   offsetof(struct Pg_User, usename), /* offset from col strt */
   0,                  /* =1 to save on disk */
   spoof_user,         /* called before read */
   (void (*)()) 0,     /* called after write */
   "The name of the Postgres user.  We overwrite this column "
   "with whatever string is in the WHERE clause.  In this way "
   "there is always a match when looking up a user name in the "
   "DB"},
  {
   "pg_user",                   /* table name */
   "usesysid",                  /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(struct Pg_User, usesysid), /* offset from col strt */
   0,                  /* =1 to save on disk */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The user ID of the user.  Currently fixed at 100."},
  {
   "pg_user",                   /* table name */
   "usecreatedb",               /* column name */
   RTA_STR,                     /* type of data */
   FT_LEN,                      /* #bytes in col data */
   offsetof(struct Pg_User, usecreatedb), /* offset from strt */
   0,                  /* =1 to save on disk */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Bool flag ('t' or 'f') to indicate whether the user can "
   "create new data bases.  Always 'f' in our system,"},
  {
   "pg_user",                   /* table name */
   "usetrace",                  /* column name */
   RTA_STR,                     /* type of data */
   FT_LEN,                      /* #bytes in col data */
   offsetof(struct Pg_User, usetrace), /* offset from col strt */
   0,                  /* =1 to save on disk */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Bool flag ('t' or 'f') to indicate whether the user can "
   "use the trace capability.  Always 'f' in our system,"},
  {
   "pg_user",                   /* table name */
   "usesuper",                  /* column name */
   RTA_STR,                     /* type of data */
   FT_LEN,                      /* #bytes in col data */
   offsetof(struct Pg_User, usesuper), /* offset from col strt */
   0,                  /* =1 to save on disk */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Bool flag ('t' or 'f') to indicate whether the user has "
   "super-user capability.  Always 'f' in our system,"},
  {
   "pg_user",                   /* table name */
   "usecatupd",                 /* column name */
   RTA_STR,                     /* type of data */
   FT_LEN,                      /* #bytes in col data */
   offsetof(struct Pg_User, usecatupd), /* offset from strt */
   0,                  /* =1 to save on disk */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Bool flag ('t' or 'f').  Always 'f' in our system,"},
  {
   "pg_user",                   /* table name */
   "passwd",                    /* column name */
   RTA_STR,                     /* type of data */
   MX_PGNAMELEN,                /* #bytes in col data */
   offsetof(struct Pg_User, passwd), /* offset from col strt */
   0,                  /* =1 to save on disk */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The encrypted password of the user.  Not implemented in "
   "the current system."},
  {
   "pg_user",                   /* table name */
   "valuntil",                  /* column name */
   RTA_STR,                     /* type of data */
   MX_PGNAMELEN,                /* #bytes in col data */
   offsetof(struct Pg_User, valuntil), /* offset from col strt */
   0,                  /* =1 to save on disk */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The expiration date of the password.  Not implemented in "
   "the current system."},
};

/* Define the table */
TBLDEF pg_userTable = {
  "pg_user",           /* table name */
  (void *) pg_user,    /* address of table */
  sizeof(struct Pg_User), /* length of each row */
  1,                   /* # rows in table */
  pg_userCols,         /* Column definitions */
  sizeof(pg_userCols) / sizeof(COLDEF), /* # columns */
  "",                  /* save file name */
  "The table of Posgres users.  We spoof this table so that "
   "any user name in a WHERE clause appears in the table as a "
   "ligitimate user with no super, createDB, trace or catupd "
   "capability."
};

/* spoof_user(): - Routine to copy user name from WHERE clause
 * to 'usename' in order to spoof the system into accepting
 * any user name as a valid user.  We normally expect a command
 * of the form:
 * SELECT usesuper FROM gp_user WHERE usename = 'XXXXXXX'
 *    What we want to do is to copy XXXXXX into the usename
 * field so there is always a match.  We do some basic checks
 * to be sure the command is of the above form.
 *
 * Input:        Name of the table 
 *               Name of the column
 *               Text of the SQL command itself
 *               Index of row used (zero indexed)
 * Output:       
 * Effects:      Copies user name to 'usename' field
 ***************************************************************/
void
spoof_user(char *tblname,
           char *colname,
           char *sqlcmd,
           int rowid)
{
  extern struct Sql_Cmd cmd;

  /* Verify that the column of interest is 'usename'. Verify
     that there is one WHERE clause. Verify that the WHERE
     relation is equality. */
  if ((strcmp(colname, "usename")) ||
      (cmd.nwhrcols != 1) || (cmd.whrrel[0] != RTA_EQ))
    return;

  /* It seems OK.  Copy the WHERE string */
  strncpy(pg_user[0].usename, cmd.whrvals[0], MX_PGNAMELEN);
  return;
}

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
COLDEF rta_columnsCols[] = {
  {
   "rta_columns",               /* table name */
   "table",                     /* column name */
   RTA_PSTR,                    /* type of data */
   MXTBLNAME,                   /* #bytes in col data */
   offsetof(COLDEF, table), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The name of the table that this column belongs to."},
  {
   "rta_columns",               /* table name */
   "name",                      /* column name */
   RTA_PSTR,                    /* type of data */
   MXCOLNAME,                   /* #bytes in col data */
   offsetof(COLDEF, name), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The name of the column.  Must be unique within a table "
   "definition but may be replicated in other tables.  The "
   "maximum string lenght of the column name is set by "
   "MXCOLNAME defined in the rta.h file."},
  {
   "rta_columns",               /* table name */
   "type",                      /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(COLDEF, type), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The data type of the column.  Types include string, "
   "integer, long, pointer, pointer to string, pointer to "
   "integer, and pointer to long.  See rta.h for more details."},
  {
   "rta_columns",               /* table name */
   "length",                    /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(COLDEF, length), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The length of the string in bytes if the column data "
   "type is a string or a pointer to a string."},
  {
   "rta_columns",               /* table name */
   "offset",                    /* column name */
   RTA_PTR,                     /* type of data */
   sizeof(void *),              /* #bytes in col data */
   offsetof(COLDEF, offset), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The number of bytes from the start of the structure "
   "to the member element defined in this entry.  Be careful "
   "in setting the offset with non word-aligned elements like "
   "single characters.  If you do no use offsetof() consider "
   "using -fpack-struct"},
  {
   "rta_columns",               /* table name */
   "flags",                     /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(COLDEF, flags), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Flags associated with the column include flags to indicate "
   "read-only status and whether or not the data should be "
   "included in the save file.  See rta.h for the associated "
   "defines and details."},
  {
   "rta_columns",               /* table name */
   "readcb",                    /* column name */
   RTA_PTR,                     /* type of data */
   sizeof(void *),              /* #bytes in col data */
   offsetof(COLDEF, readcb), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A pointer to a function that returns an integer.  If "
   "defined, the function is called before the column is "
   "read.  This function is useful to compute values only "
   "when needed."},
  {
   "rta_columns",               /* table name */
   "writecb",                   /* column name */
   RTA_PTR,                     /* type of data */
   sizeof(void *),              /* #bytes in col data */
   offsetof(COLDEF, writecb), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A pointer to a function that returns and integer.  If "
   "defined, the function is called after an UPDATE command "
   "modifies the column.  All columns in an UPDATE are "
   "modified before any write callbacks are executed.  This "
   "function is useful to effect changes requested or implied "
   "by the column definition."},
  {
   "rta_columns",               /* table name */
   "help",                      /* column name */
   RTA_PSTR,                    /* type of data */
   MXHELPSTR,                   /* #bytes in col data */
   offsetof(COLDEF, help), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A brief description of the column.  Should include "
   "limits, default value, and a description of how to set "
   "it.  Can contain at most MXHELPSTR characters."},
};

/* Define the table */
TBLDEF rta_columnsTable = {
  "rta_columns",       /* table name */
  (void *) 0,          /* address of table */
  sizeof(COLDEF),      /* length of each row */
  0,                   /* incremented as tables are added */
  rta_columnsCols,     /* Column definitions */
  sizeof(rta_columnsCols) / sizeof(COLDEF), /* # columns */
  "",                  /* save file name */
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
COLDEF rta_tablesCols[] = {
  {
   "rta_tables",                /* table name */
   "name",                      /* column name */
   RTA_PSTR,                    /* type of data */
   MXTBLNAME,                   /* #bytes in col data */
   offsetof(TBLDEF, name), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The name of the table.  This must be unique in the system. "
   " Table names can be at most MXTBLNAME characters in lenght."
   "  See rta.h for details.  Note that some table names are "
   "reserved for internal use."},
  {
   "rta_tables",                /* table name */
   "address",                   /* column name */
   RTA_PTR,                     /* type of data */
   sizeof(void *),              /* #bytes in col data */
   offsetof(TBLDEF, address), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The start address of the array of structs that makes up "
   "the table."},
  {
   "rta_tables",                /* table name */
   "rowlen",                    /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(TBLDEF, rowlen), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The length of each struct in the array of structs that "
   "makes up the table."},
  {
   "rta_tables",                /* table name */
   "nrows",                     /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(TBLDEF, nrows), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The number of rows in the table."},
  {
   "rta_tables",                /* table name */
   "cols",                      /* column name */
   RTA_PTR,                     /* type of data */
   sizeof(void *),              /* #bytes in col data */
   offsetof(TBLDEF, cols), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A pointer to an array of COLDEF structures.  There is one "
   "COLDEF for each column in the table."},
  {
   "rta_tables",                /* table name */
   "ncol",                      /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(TBLDEF, ncol), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The number of columns in the table."},
  {
   "rta_tables",                /* table name */
   "savefile",                  /* column name */
   RTA_PSTR,                    /* type of data */
   MXFILENAME,                  /* #bytes in col data */
   offsetof(TBLDEF, savefile), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The name of the file with the non-volatile contents of "
   "the table.  This file is read when the table is "
   "initialized and is written any time a column with the "
   "non-volatile flag set is modified."},
  {
   "rta_tables",                /* table name */
   "help",                      /* column name */
   RTA_PSTR,                    /* type of data */
   MXHELPSTR,                   /* #bytes in col data */
   offsetof(TBLDEF, help), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A description of the table."},
};

/* Define the table */
TBLDEF rta_tablesTable = {
  "rta_tables",        /* table name */
  (void *) 0,          /* address of table */
  sizeof(TBLDEF),      /* length of each row */
  0,                   /* It's a pseudo table */
  rta_tablesCols,      /* Column definitions */
  sizeof(rta_tablesCols) / sizeof(COLDEF), /* # columns */
  "",                  /* save file name */
  "The table of all tables in the system.  This is a pseudo "
   "table and not an array of structures like other tables."
};

/***************************************************************
 *     The rta_dbg table controls which errors generate
 * debug log messages, the priority, and the facility of the
 * syslog() messages sent.  The rta package generates no user
 * level log * messages, only debug messages.  All of the fields
 * in this table are volatile.  You will need to set the values
 * in your main program to make them seem persistent.
 * (Try something like
 *    "SQL_string("UPDATE rta_dbgconfig SET dbg ....").)
 * A callback attached to dbg_facility causes a close/reopen of
 * syslog().
 **************************************************************/

/* Allocate and initialize the table */
struct EpgDbg rtadbg = {
  1,                   /* log system errors */
  1,                   /* log rta errors */
  1,                   /* log SQL errors */
  0,                   /* no log of SQL cmds */
  1,                   /* log to syslog() only */
  LOG_ERR,             /* see sys/syslog.h */
  LOG_USER,            /* see sys/syslog.h */
  "rta"                /* see 'man openlog' */
};

/* Define the table columns */
COLDEF rta_dbgCols[] = {
  {
   "rta_dbg",                   /* table name */
   "syserr",                    /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(struct EpgDbg, syserr), /* offset 2 col strt */
   0,                  /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A non-zero value causes a call to syslog() for all system "
   "errors such as failed malloc() or save file read failures."},
  {
   "rta_dbg",                   /* table name */
   "rtaerr",                    /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(struct EpgDbg, rtaerr), /* offset 2 col strt */
   0,                  /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A non-zero value causes a call to syslog() for all errors "
   "internal to the rta package."},
  {
   "rta_dbg",                   /* table name */
   "sqlerr",                    /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(struct EpgDbg, sqlerr), /* offset 2 col strt */
   0,                  /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A non-zero value causes a call to syslog() for all SQL "
   "errors.  Such errors usually indicate a programming error "
   "in one of the user interface programs."},
  {
   "rta_dbg",                   /* table name */
   "trace",                     /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(struct EpgDbg, trace), /* offset 2 col strt */
   0,                  /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "A non-zero value causes all SQL commands to be logged.  "
   "If the command is UPDATE, the number of rows affected is "
   "also logged."},
  {
   "rta_dbg",                   /* table name */
   "target",                    /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(struct EpgDbg, target), /* offset 2 col strt */
   0,                  /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   restart_syslog,     /* called after write */
   "Sets destination of log messages.  Zero turns off all "
   "logging of errors.  One sends log messages to syslog()."
   "  Two sends log messages to stderr.  Three sends error "
   "messages to both syslog() and to stderr.  Default is one."},
  {
   "rta_dbg",                   /* table name */
   "priority",                  /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(struct EpgDbg, priority), /* offset 2 col strt */
   0,                  /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The syslog() priority.  Please see .../sys/syslog.h for "
   "the possible values.  Default is LOG_ERR."},
  {
   "rta_dbg",                   /* table name */
   "facility",                  /* column name */
   RTA_INT,                     /* type of data */
   sizeof(int),                 /* #bytes in col data */
   offsetof(struct EpgDbg, facility), /* offset 2 col strt */
   0,                  /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The syslog() facility.  Please see .../sys/syslog.h for "
   "the possible values.  Default is LOG_USER."},
  {
   "rta_dbg",                   /* table name */
   "ident",                     /* column name */
   RTA_STR,                     /* type of data */
   MXDBGIDENT,                  /* #bytes in col data */
   offsetof(struct EpgDbg, ident), /* offset 2 col strt */
   0,                  /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "The syslog() 'ident'.  Please see 'man openlog' for "
   "details.  Default is 'rta'.  An update of the target "
   "field is required for this to take effect."},
};

/***************************************************************
 * restart_syslog(): - Routine to restart or reconfigure the
 * logging facility.  Syslog is always closed and (if enabled)
 * reopened with the priority and facility sepcified in the
 * rtadbg structure.
 *
 * Input:        Name of the table 
 *               Name of the column
 *               Text of the SQL command itself
 *               Index of row used (zero indexed)
 * Output:       
 * Effects:      Copies user name to 'usename' field
 **************************************************************/
void
restart_syslog(char *tblname,
               char *colname,
               char *sqlcmd,
               int rowid)
{
  extern struct EpgDbg rtadbg;

  closelog();

  if (rtadbg.target == 1 || rtadbg.target == 3)
  {
    openlog(rtadbg.ident, LOG_ODELAY | LOG_PID, rtadbg.facility);
  }

  return;
}

/* Define the table */
TBLDEF rta_dbgTable = {
  "rta_dbg",           /* table name */
  (void *) &rtadbg,    /* address of table */
  sizeof(struct EpgDbg), /* length of each row */
  1,                   /* # rows in table */
  rta_dbgCols,         /* Column definitions */
  sizeof(rta_dbgCols) / sizeof(COLDEF), /* # columns */
  "",                  /* save file name */
  "Configure of debug logging.  A callback on the 'target' "
   "field closes and reopens syslog().  None of the values "
   "in this table are saved to disk.  If you want non-default "
   "values you need to change the rta source or do an "
   "SQL_string() to set the values when you initialize your "
   "program."
};

/***************************************************************
 *     The rta_stats table contains usage and error statistics
 * which might be of interest to developers.  All fields are
 * of type long, are read-only, and are set to zero by
 * rta_init(). 
 **************************************************************/

/* Allocate and initialize the table */
struct EpgStat rtastat = {
  (long long) 0,       /* count of failed OS calls. */
  (long long) 0,       /* count of internal rta failures. */
  (long long) 0,       /* count of SQL failures. */
  (long long) 0,       /* count of authorizations. */
  (long long) 0,       /* count of UPDATE requests */
  (long long) 0,       /* count of SELECT requests */
};

/* Define the table columns */
COLDEF rta_statCols[] = {
  {
   "rta_stat",                  /* table name */
   "nsyserr",                   /* column name */
   RTA_LONG,                    /* type of data */
   sizeof(long long),           /* #bytes in col data */
   offsetof(struct EpgStat, nsyserr), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Count of failed OS calls."},
  {
   "rta_stat",                  /* table name */
   "nrtaerr",                   /* column name */
   RTA_LONG,                    /* type of data */
   sizeof(long long),           /* #bytes in col data */
   offsetof(struct EpgStat, nrtaerr), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Count of internal rta failures."},
  {
   "rta_stat",                  /* table name */
   "nsqlerr",                   /* column name */
   RTA_LONG,                    /* type of data */
   sizeof(long long),           /* #bytes in col data */
   offsetof(struct EpgStat, nsqlerr), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Count of SQL failures."},
  {
   "rta_stat",                  /* table name */
   "nauth",                     /* column name */
   RTA_LONG,                    /* type of data */
   sizeof(long long),           /* #bytes in col data */
   offsetof(struct EpgStat, nauth), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Count of DB authorizations.  This is a good estimate "
   "to the total number of connections."},
  {
   "rta_stat",                  /* table name */
   "nselect",                   /* column name */
   RTA_LONG,                    /* type of data */
   sizeof(long long),           /* #bytes in col data */
   offsetof(struct EpgStat, nselect), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Count of SELECT commands."},
  {
   "rta_stat",                  /* table name */
   "nupdate",                   /* column name */
   RTA_LONG,                    /* type of data */
   sizeof(long long),           /* #bytes in col data */
   offsetof(struct EpgStat, nupdate), /* offset 2 col strt */
   RTA_READONLY,       /* Flags for read-only/disksave */
   (void (*)()) 0,     /* called before read */
   (void (*)()) 0,     /* called after write */
   "Count of UPDATE commands."},
};

/* Define the table */
TBLDEF rta_statTable = {
  "rta_stat",          /* table name */
  (void *) &rtastat,   /* address of table */
  sizeof(struct EpgStat), /* length of each row */
  1,                   /* # rows in table */
  rta_statCols,        /* Column definitions */
  sizeof(rta_statCols) / sizeof(COLDEF), /* # columns */
  "",                  /* save file name */
  "Usage and error counts for the rta package."
};
