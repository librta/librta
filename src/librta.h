/***************************************************************
 * librta library         
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * librta.h  -- DB API for your internal structures and tables
 **************************************************************/

/** ************************************************************
 * - Preamble:
 * "librta" is a specialized memory resident data base interface.
 * It is not a stand-alone server but a library which attaches
 * to a program and offers up the program's internal structures
 * and arrays as data base tables.  It uses a subset of the 
 * Postgres protocol and is compatible with the Postgres bindings
 * for "C", PHP, and the Postgres command line tool, psql.
 *
 *    This file contains the defines, structures, and function
 * prototypes for the 'librta' package.
 *
 * INDEX: 
 *        - Preamble
 *        - Introduction and Purpose
 *        - Limits
 *        - Data Structures
 *        - Subroutines
 *        - librta UPDATE and SELECT syntax
 *        - librta INSERT and DELETE syntax
 *        - Internal DB tables
 *        - List of all error messages
 *        - How to code callback routines
 **************************************************************/

/** ************************************************************
 * - Introduction and Purpose:
 *    One of the problems facing Linux is the lack of real-time
 * access to status, statistics, and configuration of a service
 * once it has started.  We assume that to configure an
 * application we will be able to ssh into the box, vi the /etc
 * configuration file, and do a 'kill -1' on the process.  Real
 * time status and statistics are things Linux programmers don't
 * even think to ask for.  The problem of run time access is 
 * particularly pronounced for network appliances where ssh is
 * not available or might not be allowed.
 *    Another problem for appliance designers is that more than
 * one type of user interface may be required.  Sometimes
 * a customer requires that *no* configuration information be
 * sent over an Ethernet line which transports unsecured user
 * data.  In such a case the customer may turn off the web
 * interface and require that configuration, status, and
 * statistics be sent over an RS-232 serial line.  The VGA
 * console, SNMP MIBs,  and LDAP are also popular management
 * interfaces.
 *     The librta package helps solve these problems by giving
 * real-time access to the data structures and arrays inside a
 * running program.  With minimal effort, we make a program's
 * data structures appear as Postgres tables in a Postgres data
 * base.  
 *    For example, say you have a structure for TCP connections 
 * defined as:
 *    struct tcpconn {
 *      int   fd;       // conn's file descriptor
 *      int   lport;    // local port number
 *      int   dport;    // destination port number
 *      long  nsbytes;  // number of sent bytes
 *      long  nrbytes;  // number of received bytes
 *      long  nread;    // number of reads on the socket 
 *      long  nwrite;   // number of writes on the socket
 *    };
 *   
 * You might then define an array of these structures as:
 *    struct tcpconn Conns[MX_CONN];
 *
 *     The librta package allows any programming language with a 
 * Postgres binding to query your table of TCP connections....
 *    SELECT lport, dport FROM Conns WHERE fd != -1;
 *    UPDATE Conns SET dport = 0 WHERE fd = -1;
 *
 *     A data base API for all of your program's configuration,
 * status, and statistics makes debugging easier since you can
 * view much more of your program's state with simple Postgres
 * tools.  A data base API makes building user interface programs
 * easier since there are Postgres bindings for PHP, Tcl/Tk,
 * Perl, "C", as well as many more.
 *     A data base API can help speed development.  Carefully
 * defining the tables to be used by the UI programs lets the
 * core application team build the application while the UI
 * developers work on the web pages, wizards, and MIBs of the
 * various UI programs.  
 *
 *     Some effort is required.  In order to make your arrays
 * and structures available to the data base API, you need to
 * tell librta about the tables and columns in the data base. 
 * Table information includes things like the name, start
 * address, number of rows and the length of each row.  Column
 * information includes things like the associated table name,
 * the column name, the column's data type, and whether we want
 * any special functions called when the column is read or
 * written (callbacks).
 *
 *   This document describes the API offered by the librta package.
 **************************************************************/

#ifndef RTA_H
#define RTA_H

/***************************************************************
 * - Limits:
 *     Here are the defines which describe the internal limits
 * set in the librta package.  You are welcome to change these 
 * limits; just be sure to recompile the librta package using 
 * your new settings.
 **************************************************************/

#include <limits.h>             /* for PATH_MAX */

        /** Maximum number of tables allowed in the system.
         * Your data base may not contain more than this number
         * of tables. */
#define RTA_MX_TBL        (500)

        /** Maximum number of columns allowed in the system.
         * Your data base may not contain more than this number
         * of columns. */
#define RTA_MX_COL       (2500)

        /** Maximum number of characters in a column name, table
         * name, and in help.  See RTA_TBLDEF and RTA_COLDEF below. */
#define RTA_MXCOLNAME     (100)
#define RTA_MXTBLNAME     (100)
#define RTA_MXHELPSTR    (1000)
#define RTA_MXFILENAME   PATH_MAX

        /** Maximum number of characters in the 'ident' field of
         * the openlog() call.  See the rta_dbg table below. */
#define RTA_MXDBGIDENT     (20)

        /** Maximum line size.  SQL commands in save files may
         * contain no more than RTA_MX_LN_SZ characters.  Lines with
         * more than RTA_MX_LN_SZ characters are silently truncated
         * to RTA_MX_LN_SZ characters. */
#define RTA_MX_LN_SZ     (2048)

    /* Maximum number of columns allowed in a table */
#define RTA_NCMDCOLS     (1000)

/***************************************************************
 * - Data Structures:
 *     Each column and table in the data base must be described
 * in a data structure.  Here are the data structures and 
 * associated defines to describe tables and columns.
 **************************************************************/

          /** The column definition (RTA_COLDEF) structure describes
           * one column of a table.  A table description has an
           * array of RTA_COLDEFs to describe the columns in the
           * table. */
typedef struct
{

          /** The name of the table that has this column. */
  char    *table;

          /** The name of the column.  Must be at most RTA_MXCOLNAME
           * characters in length and must be unique within a
           * table.  The same column name may be used in more
           * than one table. */
  char    *name;

          /** The data type of the column.  Must be int, long,
           * string, pointer to void, pointer to int, pointer
           * to long, or pointer to string.  The DB types are
           * defined immediately following this structure. */
  int      type;

          /** The number of bytes in the string if the above
           * type is RTA_STR or RTA_PSTR.  The length includes
           * the null at the end of the string.  */
  int      length;

          /** Number of bytes from the start of the structure to
           * this column.  For example, a structure with an int,
           * a 20 character string, and a long, would have the
           * offset of the long set to 24.  Use of the function
           * offsetof() is encouraged.  If you have structure
           * members that do not start on word boundaries and
           * you do not want to use offsetof(), then consider
           * using -fpack-struct with gcc. */
  int      offset;

          /** Boolean flags which describe attributes of the
           * columns.  The flags are defined after this
           * structure and include a "read-only" flag and a
           * flag to indicate that updates to this column
           * should cause a table save.  (See table savefile
           * described below.)  */
  int      flags;

          /** Read callback.  This routine is called before the
           * column value is used.  Input values include the
           * table name, the column name, the input SQL command,
           * a pointer to the row affected, and the (zero indexed)
           * row number for the row that is being read.  It
           * returns a 0 on success and non-zero on failure.
           * This routine is called *each* time the column is
           * read so the following would produce two calls:
           * SELECT intime FROM inns WHERE intime >= 100;   */
  int      (*readcb) (char *tbl, char *column, char *SQL, void *pr,
    int row_num);

          /** Write callback.  This routine is called after an
           * UPDATE in which the column is written. Input values
           * include the table name, the column name, the SQL
           * command, a pointer to the row affected, the (zero
           * indexed) row number of the modified row, and a pointer
           * to a copy of the row before any modifications.  See the
           * callback section below.
           * This routine is called only once after all column
           * updates have occurred.  For example, if there were
           * a write callback attached to the addr column, the
           * following SQL statement would cause the execution
           * of the write callback once after both mask and addr
           * have been written:
           * UPDATE ethers SET mask="255.255.255.0", addr = \
           *     "192.168.1.10" WHERE name = "eth1";
           * The callback is called for each row modified. 
           * A callback returns zero on success and non-zero on
           * failure.  On failure, the table's row is restored
           * to it's initial values and an SQL error is returned
           * to the client.  The error is TRIGGERED ACTION EXCEPTION */
  int      (*writecb) (char *tbl, char *column, char *SQL, void *pr,
                       int row_num,  void *poldrow);

          /** A brief description of the column.  This should
           * include the meaning of the data in the column, the
           * limits, if any, and the default values.  Include
           * a brief description of the side effects of changes.
           * This field is particularly important for tables 
           * which are part of the "boundary" between the UI
           * developers and the application programmers.  */
  char    *help;
}
RTA_COLDEF;

        /** The data types.
         * String refers to an array of char.  The 'length' of
         * column must contain the number of bytes in the array.
         */
#define RTA_STR          0

        /** Pointer to void.  Use for generic pointers */
#define RTA_PTR          1

        /** Integer.  This is the compiler/architecture native
         * integer.  On Linux/gcc/Pentium an integer is 32 bits.
         */
#define RTA_INT          2

        /** Long.  This is the compiler/architecture native
         * long long.  On Linux/gcc/Pentium a llong is 64
         * bits.  Define 'llong' to suit your needs.  */
typedef long long llong;
#define RTA_LONG         3

        /** Pointer to string.  Pointer to an array of char, or
         * a (**char).  Note that the column length should be
         * the number of bytes in the string, not sizeof(char *).
         */
#define RTA_PSTR         4

        /** Pointers to int and long.  */
#define RTA_PINT         5
#define RTA_PLONG        6

        /** Float and pointer to float */
#define RTA_FLOAT        7
#define RTA_PFLOAT       8

#define RTA_SHORT        9
#define RTA_UCHAR       10
#define RTA_DOUBLE      11

#define RTA_MXCOLTYPE       (RTA_DOUBLE)

        /** The boolean flags.  
         * If the disksave bit is set any writes to the column
         * causes the table to be saved to the "savefile".  See
         * savefile described in the RTA_TBLDEF section below. */
#define RTA_DISKSAVE     (1<<0)

        /** If the readonly flag is set, any writes to the 
         * column will fail and a debug log message will be 
         * sent.  (For unit test you may find it very handy to
         * leave this bit clear to get better test coverage of
         * the corner cases.)   */
#define RTA_READONLY     (1<<1)

        /** The table definition (RTA_TBLDEF) structure describes
         * a table and is passed into the DB system by the
         * rta_add_table() subroutine.  */
typedef struct
{
        /** The name of the table.  Must be less than than
         * RTA_MXTLBNAME characters in length.  Must be unique
         * within the DB.  */
  char    *name;

        /** Address of the first element of the first row of
         * the array of structs that make up the table.  */
  void    *address;

        /** The number of bytes in each row of the table.  
         * This is usually a sizeof() of the structure 
         * associated with the table.   (The idea is that we
         * can get to data element E in row R with ...
         *    data = *(address + (R * rowlen) + offset(E)) */
  int      rowlen;

        /** Number of rows in the table.   */
  int      nrows;

        /** An 'iterator' on the rows of the data.  This is
         * useful if you want to have a linked list (or other
         * arrangement) instead of a linear array of struct.
         * Your iterator should return a pointer to the first
         * row when the input is NULL and should return a NULL
         * when asked for the row after the last row.   The rowid
         * is the number of the row desired.  */
  void    *(*iterator) (void *cur_row, void *it_info, int rowid);

        /** A pointer to any kind of information that the 
         * caller wants returned with each iterator call.  
         * For example, you may wish to have one iterator 
         * for all of your linked lists.  You could pass in
         * a unique identifier for each table so the function
         * can handle each one as appropriate. */
  void    *it_info;

        /** INSERT callback.  This routine is called to insert a
         * a new row into a table.  This routine should only be
         * used for tables in which the rows are malloc'ed one at
         * a time.  The insert callback is given the name of the
         * table, the SQL of the original INSERT command, and a
         * pointer to a newly allocated row for that table.  The
         * callback needs to attach the row to the table using
         * the technique (linked list, b-tree) suitable for that
         * table.  On success, the callback should return the
         * zero-indexed row number for the new row or, on failure,
         * the callback should return -1.  */
  int    (*insertcb) (char *tbl, char *SQL, void *pr);

        /** DELETE callback.  This routine is called to delete a
         * row from a table.  This routine is appropriate for 
         * tables that have rows that are malloc'ed.  It is up to
         * the delete callback to unlink the row from the table
         * and to FREE ALL MEMORY MALLOC'ED for the row. This
         * callback has no return value and always succeeds. */
  void   (*deletecb) (char *tbl, char *SQL, void *pr);

        /** An array of RTA_COLDEF structures which describe each
         * column in the table.  These must be in statically
         * allocated memory since the librta system references
         * them while running.  */
  RTA_COLDEF  *cols;

        /** The number of columns in the table.  That is, the
         * number of RTA_COLDEFs defined by 'cols'.  */
  int      ncol;

        /**  Save file.  Path and name of a file which stores
         * the non-volatile part of the table.  The file has
         * all of the UPDATE statements needed to rebuild the
         * table.  The file is rewritten in its entirety each
         * time a 'savetodisk' column is updated.  No file
         * save is attempted if the savefile is blank. */
  char    *savefile;

        /**  Help text.  A description of the table, how it is
         * used, and what its intent is.  A brief note to
         * describe how it relate to other parts of the system
         * and description of important callbacks is nice 
         * thing to include here.  */
  char    *help;
}
RTA_TBLDEF;

/***************************************************************
 * - Subroutines
 * Here is a summary of the few routines in the librta API:
 *    rta_dbcommand()  - I/F to Postgres clients
 *    rta_add_table()  - add a table and its columns to the DB
 *    rta_SQL_string() - execute an SQL statement in the DB
 *    rta_save()       - save a table to a file
 *    rta_load()       - load a table from a file
 *
 **************************************************************/

/** ************************************************************
 * rta_dbcommand():  - Depacketize and execute Postgres commands.
 *
 * The main application accepts TCP connections from Postgres
 * clients and passes the stream of bytes (encoded SQL requests)
 * from the client into the librta system via this routine.  If the
 * input buffer contains a complete command, it is executed, nin
 * is decrement by the number of bytes consumed, and RTA_SUCCESS
 * is returned.  If there is not a complete command, RTA_NOCMD
 * is returned and no bytes are removed from the input buffer.
 * If a command is executed, the results are encoded into the
 * Postgres protocol and placed in the output buffer.  When the
 * routine is called the input variable, nout, has the number of
 * free bytes available in the output buffer, out.  When the
 * routine returns nout has been decremented by the size of the
 * response placed in the output buffer.  An error message is
 * generated if the number of available bytes in the output 
 * buffer is too small to hold the response from the SQL command.
 * 
 * Input:  cmd - the buffer with the Postgres packet
 *         nin - on entry, the number of bytes in 'cmd',
 *               on exit, the number of bytes remaining in cmd
 *         out - the buffer to hold responses back to client
 *         nout - on entry, the number of free bytes in 'out'
 *               on exit, the number of remaining free bytes
 * Return: RTA_SUCCESS   - executed one command
 *         RTA_NOCMD     - input did not have a full cmd
 *         RTA_CLOSE     - client requests an orderly close
 *         RTA_NOBUF     - insufficient output buffer space
 **************************************************************/
int      rta_dbcommand(char *, int *, char *, int *);

/** ************************************************************
 * rta_add_table():  - Register a table for inclusion in the
 * DB interface.  Adding a table allows external Postgres
 * clients access to the table's content.
 *     Note that the RTA_TBLDEF structure must be statically
 * allocated.  The DB system keeps just the pointer to the table
 * and does not copy the information.  This means that you can
 * change the contents of the table definition by changing the
 * contents of the RTA_TBLDEF structure.  This might be useful if
 * you need to allocate more memory for the table and change its
 * row count and address.
 *    An error is returned if another table by the same name 
 * already exists in the DB or if the table is defined without
 * any columns.
 *    If a 'savefile' is specified, it is loaded.  (See the
 * rta_load() command below for more details.)
 * 
 * Input:  ptbl          - pointer to the RTA_TBLDEF to add
 * Return: RTA_SUCCESS   - table added
 *         RTA_ERROR     - error
 **************************************************************/
int      rta_add_table(RTA_TBLDEF *);

/** ************************************************************
 * rta_SQL_string():  - Execute single SQL command
 *
 * Executes the SQL command placed in the null-terminated string,
 * cmd.  The results are encoded into the Postgres protocol and
 * placed in the output buffer.  When the routine is called the
 * input variable, nout, has the number of free bytes available
 * in the output buffer, out.  When the routine returns nout has
 * been decremented by the size of the response placed in the
 * output buffer.  An error message is generated if the number
 * of available bytes in the output buffer is too small to hold
 * the response from the SQL command.
 *     This routine may be most useful when updating a table
 * value in order to invoke the write callbacks.  (The output
 * buffer has the results encoded in the Postgres protocol and
 * might not be too useful directly.)
 *
 * Input:  cmd - the buffer with the SQL command,
 *         nin - number of bytes in cmd,
 *         out - the buffer to hold responses back to client,
 *         nout - on entry, the number of free bytes in 'out'
 *               on exit, the number of remaining free bytes
 * Return: 
 **************************************************************/
void     rta_SQL_string(char *, int, char *, int *);


/** ************************************************************
 * rta_config_dir():  - sets the default path to the savefiles.
 *
 * The string pointed to by configdir is saved and is prepended
 * to the savefile names for tables with savefiles.  This call
 * should be used before loading your application tables.  It
 * is intended to make it simpler for applications which let
 * the user specify an configuration directory on the command
 * line.
 *  If the savefile uses an absolute path (starting with '/')
 * it is not prepended with the configuration directory.
 *
 * Return: RTA_SUCCESS   - config path set
 *         RTA_ERROR     - error, (not a valid directory?)
 **************************************************************/
int rta_config_dir(char *configdir);



/** ************************************************************
 * rta_save():  - Save table to file.   Saves all "savetodisk"
 * columns to the path/file specified.  Only savetodisk columns
 * are saved.  The resultant file is a list of UPDATE commands
 * containing the desired data.  There is one UPDATE command for
 * each row in the table.
 *     This routine tries to minimize exposure to corrupted 
 * save files by opening a temp file in the same directory as
 * the target file.  The data is saved to the temp file and the
 * system call rename() is called to atomically move the temp
 * to the save file.  Errors are generated if rta_save can not
 * open the temp file or is unable to rename() it.
 *     As a general warning, note that any disk I/O can cause
 * a program to block briefly and so saving and loading tables
 * might cause your program to block.
 * 
 * Input:  ptbl   - pointer to the RTA_TBLDEF structure for the 
 *                  table to save
 *         fname  - null terminated string with the path and
 *                  file name for the stored data.
 * Return: RTA_SUCCESS   - table saved
 *         RTA_ERROR     - some kind of error
 **************************************************************/
int      rta_save(RTA_TBLDEF *, char *);

/** ************************************************************
 * rta_load():  - Load a table from a file of UPDATE commands.
 * The file format is a series of UPDATE commands with one 
 * command per line.  Any write callbacks are executed as the
 * update occurs.  
 * 
 * Input:  ptbl   - pointer to the table to be loaded
 *         fname  - string with name of the load file
 *
 * Return: RTA_SUCCESS   - table loaded
 *         RTA_ERROR     - could not open the file specified
 **************************************************************/
int      rta_load(RTA_TBLDEF *, char *);

    /* successfully executed request or command */
#define RTA_SUCCESS   (0)

    /* input did not have a full command */
#define RTA_NOCMD     (1)

    /* encountered an internal error */
#define RTA_ERROR     (2)

    /* DB client requests a session close */
#define RTA_CLOSE     (3)

    /* Insufficient output buffer space */
#define RTA_NOBUF     (4)


/** ************************************************************
 * - librta UPDATE and SELECT syntax
 *     librta IS AN API, *NOT* A DATABASE!
 * Neither the librta UPDATE nor the librta SELECT adhere to the
 * Postgres equivalents.  Joins are not allowed, and the WHERE
 * clause supports only the AND relation.  There are no locks
 * or transactions.
 *
 * SELECT:
 *    SELECT column_list FROM table [where_clause] [limit_clause]
 *
 *    SELECT supports multiple columns, '*', LIMIT, and OFFSET.
 * At most RTA_MXCMDCOLS columns can be specified in the select list
 * or in the WHERE clause.  LIMIT restricts the number of rows
 * returned to the number specified.  OFFSET skips the number of
 * rows specified and begins output with the next row.
 * 'column_list' is a '*' or 'column_name [, column_name ...]'.
 * 'where_clause' is 'col_name = value [AND col_name = value ..]'
 * in which all col=val pairs must match for a row to match.
 *     LIMIT and OFFSET are very useful to prevent a buffer
 * overflow on the output buffer of rta_dbcommand().  They are also
 * very useful for web based user interfaces in which viewing
 * the data a page-at-a-time is desirable.
 *     Column and table names are case sensitive and may not be
 * one of the reserved words.  The reserved words are: AND, FROM, 
 * LIMIT, OFFSET, SELECT, SET, UPDATE, and WHERE.  Reserved 
 * words are *not* case sensitive.  You may use lower case
 * reserved words in your SQL statements if you wish.
 *    Comparison operator in the WHERE clause include =, >=,
 * <=, >, and <.
 *    You can use a reserved word, like OFFSET, as a column name
 * but you will need to quote it whenever you reference it in an
 * SQL command (SELECT "offset" FROM tunings ...).   Strings 
 * may contain any of the !@#$%^&*()_+-={}[]\|:;<>?,./~`
 * characters.  If a string contains a double quote, use a 
 * single quote to wrap it (eg 'The sign says "Hi mom!"'), and
 * use double quotes to wrap string with embedded single quotes.
 *
 *     Examples:
 * SELECT * FROM rta_tables
 *
 * SELECT destIP FROM conns WHERE fd != 0
 *
 * SELECT destIP FROM conns WHERE fd != 0 AND lport = 80
 *
 * SELECT destIP, destPort FROM conns \
 *       WHERE fd != 0 \
 *       LIMIT 100 OFFSET 0
 * 
 * SELECT destIP, destPort FROM conns \
 *       WHERE fd != 0 \
 *       LIMIT 100 OFFSET 0
 *
 *
 * UPDATE:
 *    UPDATE table SET update_list [where_clause] [limit_clause]
 *
 *    UPDATE writes values into a table.  The update_list is of
 * the form 'col_name = val [, col_name = val ...].  The WHERE
 * and LIMIT clauses are as described above.  
 *    An update invokes write callbacks on the affected columns.
 * All data in the row is written before the callbacks are 
 * called.  
 *    The LIMIT clause for updates is not standard Postgres SQL,
 * but can be really useful for stepping through a table one row
 * at a time.  To change only the n'th row of a table, use a
 * limit clause like 'LIMIT 1 OFFSET n' (n is zero-indexed).
 *
 *    Examples:
 * UPDATE conn SET lport = 0;
 *
 * UPDATE ethers SET mask = "255.255.255.0", \ 
 *                   addr = "192.168.1.10"   \ 
 *       WHERE name = "eth0"
 *
 * UPDATE conn SET usecount = 0 WHERE fd != 0 AND lport = 21
 *
 **************************************************************/

/** ************************************************************
 * - librta INSERT and DELETE syntax
 *     librta IS AN API, *NOT* A DATABASE!
 * Neither the librta INSERT nor the librta DELETE adhere to the
 * Postgres equivalents.  An insert requires both the column list
 * and the values section and does not support "default" specified
 * in the command.  The librta DELETE does not support "ONLY" or "USING"
 * clauses, and the WHERE clause supports only the AND relation.
 * There are no locks or transactions.
 *
 * INSERT:
 *    INSERT INTO table ( column_list ) VALUES ( value_list )
 *
 *    INSERT is used to allocate and add a new row into a table.
 * Only the syntax given above is supported, and an error is
 * returned if the number or type of columns in the column_list do
 * not match the number and types of value in the value_list.
 *
 * INSERT and DELETE require callback routines.  The syntax for
 * the callbacks and some general guidelines for callback routines
 * is given later in this document.
 *
 *     INSERT examples:
 * Insert a new row letting the insert callback set the values
 * INSERT INTO demotbl ( ) VALUES ( )
 *
 * Insert a new row setting the dlplong column to 5.
 * INSERT INTO demotbl ( dlplong ) VALUES ( 5 )
 *
 * As above but also set dlstr to "hello, world"
 * INSERT INTO demotbl ( dlstr, dlplong ) VALUES ( "hello,world", 5 )
 *
 *
 * DELETE:
 *    DELETE FROM table [where_clause] [limit_clause]
 *
 *    DELETE removes rows from a table and frees any memory
 * allocated for the rows.  The WHERE and LIMIT clauses are as
 * described in the select section above.  
 *
 *    Examples:
 * Delete all rows in the demotbl table.
 * DELETE FROM demotbl
 *
 * Delete all rows where the dlplong is equal to 5
 * DELETE FROM demotbl WHERE dlplong = 5
 *
 * Delete the fourth and fifth rows.  (Table are zero indexed.)
 * DELETE FROM demotbl LIMIT 2 OFFSET 3
 *
 **************************************************************/

/** ************************************************************
 * - Internal DB tables
 *     librta has four tables visible to the application:
 *  rta_tables:      - a table of all tables in the DB
 *  rta_columns:     - a table of all columns in the DB
 *  rta_logconfig:   - controls what gets logged from librta
 *  rta_stats:       - simple usage and error statistics
 *
 *     The rta_tables table gives SQL access to all internal and
 * registered tables.  The data in the table is exactly that of
 * the RTA_TBLDEF structures registered with rta_add_table().  This
 * table is used for the generic table viewer and table editor 
 * applications used mostly for application debugging.  The
 * columns of rta_tables are:
 *     name      - the name of the table
 *     address   - the start address of the table in memory
 *     rowlen    - number of bytes in each row of the table
 *     nrows     - number of rows in the table
 *     cols      - pointer to array of column definitions
 *     ncol      - number of columns in the table
 *     iterator  - subroutine to advance from one row to next
 *     it_info   - transparent data for the iterator
 *     savefile  - the file used to store non-volatile columns
 *     help      - a description of the table
 *
 *     The rta_columns table has the column definitions of all
 * columns in the DB.  The data in the table is exactly that of
 * the RTA_COLDEF structures registered with rta_add_table().  This
 * table is used for the generic table viewer and table editor
 * applications used mostly for application debugging.  The
 * columns of rta_columns are:
 *     table     - the name of the column's table
 *     name      - name of the column
 *     type      - column's data type
 *     length    - number of bytes columns data type
 *     offset    - number of bytes from start of structure
 *     flags     - Bit field for 'read-only' and 'savetodisk'
 *     readcb    - pointer to subroutine called before reads
 *     writecb   - pointer to subroutine called after writes
 *     help      - a description of the column
 *
 *     The rta_dbgconfig table controls which errors generate
 * debug log messages.  See the logging section below for the
 * exact mapping.  The librta package generates no user level log
 * messages, only debug messages.  All of the fields in this 
 * table are volatile.  You will need to set the values in your
 * main program to make them seem persistent.  (Try something
 * like "rta_SQL_string("UPDATE rta_dbgconfig SET dbg ....").)
 * The columns of rta_dbgconfig are:
 *     syserr - integer, 0 means no log, 1 means log.
 *              This logs OS call errors like malloc()
 *              failures.  Default is 1.
 *     rtaerr - integer, 0 means no log, 1 means log.
 *              Enables logging of errors internal to the
 *              librta package itself.  Default is 1.
 *     sqlerr - integer, 0 means no log, 1 means log.
 *              Log any SQL request which generates an
 *              error reply.  Error replies occur if an SQL
 *              request is malformed or if it requests a
 *              non-existent table or column.  Default is 1.
 *              (SQL errors are usually client programming
 *               errors.)
 *     trace  - integer, 0 means no log, 1 means log all
 *              SQL requests.  Default is 0.
 *     target - 0:  disable all debug logging
 *              1:  log debug messages to syslog()
 *              2:  log debug messages to stderr
 *              3:  log to both syslog() and stderr
 *              The default is 1.  Setting the facility
 *              causes a close and an open of syslog().
 *     priority - integer.  Syslog() requires a priority as
 *              part of all log messages.  This specifies
 *              the priority to use when sending librta debug
 *              messages.  Changes to this do not take
 *              effect until dbg_target is updated.
 *              0:  LOG_EMERG
 *              1:  LOG_ALERT
 *              2:  LOG_CRIT
 *              3:  LOG_ERR
 *              4:  LOG_WARNING
 *              5:  LOG_NOTICE
 *              6:  LOG_INFO
 *              7:  LOG_DEBUG
 *              Default is 3.
 *     facility - integer.  Syslog() requires a facility as
 *              part of all log messages.  This specifies
 *              the facility to use when sending librta debug
 *              messages.  It is best to use the defines in
 *              .../sys/syslog.h to set this.  The default
 *              is LOG_USER.  Changes to this do not take
 *              effect until dbg_target is updated.
 *     ident  - string.  Syslog() requires an 'ident' string as
 *              part of all log messages.  This specifies
 *              the ident string to use when sending librta debug
 *              messages.  This is normally set to the process
 *              or command name.  The default is "librta".  Changes
 *              to this do not take effect until dbg_target
 *              is updated.  This can be at most RTA_MXDBGIDENT
 *              characters in length.
 *
 *     The rta_stat table contains usage and error statistics
 * which might be of interest to developers.  All fields are
 * of type long, are read-only, and are set to zero by the 
 * first call to rta_add_table.  The columns of rta_stats are:
 *     nsyserr    - count of failed OS calls. 
 *     nrtaerr    - count of internal librta failures.
 *     nsqlerr    - count of SQL failures.
 *     nauth      - count of authorizations. (==#connections)
 *     nupdate    - count of UPDATE or file write requests
 *     nselect    - count of SELECT or file read requests
 *     ninsert    - count of INSERT commands
 *     ndelete    - count of DELETE commands
 *
 **************************************************************/

/** ************************************************************
 * - List of all messages
 *      There are two types of error messages available in the
 * librta package.  The first type is the error messages returned
 * as part of an SQL request.  The messages of this type are:
 *
 * 1)  "ERROR:  Relation '%s' does not exist"
 *      This reply indicates that a table requested in a SELECT
 *      UPDATE, or where clause does not exist.  The %s is 
 *      replaced by the name of the requested table.
 * 2)  "ERROR:  Attribute '%s' not found"
 *      This reply indicates that a column requested in a SELECT
 *      UPDATE, or where clause does not exist.  The %s is 
 *      replaced by the name of the requested column.
 * 3)  "ERROR:  SQL parse error"
 *      This reply indicates a mal-formed SQL request or a
 *      mis-match in the types of data in a where clause or in 
 *      an update list.
 * 4)  "ERROR:  Output buffer full"
 *      This reply indicates that the size of the response to
 *      a request exceeds the size of the output buffer.  See
 *      rta_dbcommand() and the 'out' and 'nout' parameters.  This
 *      error can be avoided with a large enough output buffer
 *      or, preferably, with the use of LIMIT and OFFSET.
 * 5)  "ERROR:  String too long for '%s'"
 *      This reply indicates that an update to a column of type
 *      string or pointer to string would have exceeded the
 *      width of the column.  The %s is replaced by the column
 *      name.
 * 6)  "ERROR:  Can not update read-only column '%s'"
 *      This reply indicates that an attempt to update a column
 *      marked as read-only.  The %s is replaced by the column
 *      name.
 * 7)  "ERROR:  Failed callback on column '%s'"
 *      This reply indicates that a read or write callback 
 *      failed.
 * 8)  "DELETE not available on relation '%s'"
 * 9)  "INSERT not available on relation '%s'"
 *      These replies indicate that an attempt was made to insert
 *      or delete a row from a table that does not have insert and
 *      delete capabilities.
 * 10) "Failed INSERT on relation '%s'"
 *      The insert of a row has failed.  Syntax error sare usually
 *      captured by other error messages leaving this message to
 *      imply that the application itself found something wrong
 *      with the values in the INSERT request.
 *
 *     The other type of error messages are internal debug
 * messages.  Debug messages are logged using the standard
 * syslog() facility available on all Linux systems.  The
 * default syslog "facility" used is LOG_USER but this can be
 * changed by setting 'facility' in the rta_dbg table.
 *     You are welcome to modify syslogd in order to do post
 * processing such as generating SNMP traps off these debug
 * messages.  All error messages of this type are send to
 * syslog() as:    "rta[PID]: FILE LINE#: error_message",
 * where PID, FILE, and LINE# are replaced by the process ID,
 * the source file name, and the line number where the error
 * was detected.
 *     Following are the defines used to generate these debug
 * and error messages.   The "%s %d" at the start of each 
 * error string is replaced by the file name and line number
 * where the error is detected.  */

        /** "System" errors */
#define Er_No_Mem    "%s %d: Can not allocate memory"
#define Er_No_Save   "%s %d: Table '%s' save failure.  Can not open %s"
#define Er_No_Load   "%s %d: Table '%s' load failure.  Can not open %s"

        /** "RTA" errors */
#define Er_Max_Tbls  "%s %d: Too many tables in DB"
#define Er_Max_Cols  "%s %d: Too many columns in DB"
#define Er_Tname_Big "%s %d: Too many characters in table name: %s"
#define Er_Cname_Big "%s %d: Too many characters in column name: %s"
#define Er_Hname_Big "%s %d: Too many characters in help text: %s"
#define Er_Tbl_Dup   "%s %d: DB already has table named: %s"
#define Er_Col_Dup   "%s %d: Table '%s' already has column named: %s"
#define Er_Col_Type  "%s %d: Column contains an unknown data type: %s"
#define Er_Col_Flag  "%s %d: Column contains unknown flag data: %s"
#define Er_Col_Name  "%s %d: Incorrect table in column definition: %s"
#define Er_Cmd_Cols  "%s %d: Too many columns in table: %s"
#define Er_No_Space  "%s %d: Not enough buffer space"
#define Er_Reserved  "%s %d: Table or column is a reserved word: %s"

        /** "SQL" errors */
#define Er_Bad_SQL   "%s %d: SQL parse error: %s"
#define Er_Readonly  "%s %d: Attempt to update readonly column: %s"

        /* SQL errors to the front ends */
#define E_NOTABLE    "Relation '%s' does not exist"
#define E_NOCOLUMN   "Attribute '%s' not found"
#define E_BADPARSE   "SQL parse error",""
#define E_BIGSTR     "String too long for '%s'"
#define E_NOWRITE    "Can not update read-only column '%s'"
#define E_FULLBUF    "Output buffer full",""
#define E_BADTRIG    "Failed callback on column '%s'"
#define E_NODELETE   "DELETE not available on relation '%s'"
#define E_NOINSERT   "INSERT not available on relation '%s'"
#define E_BADINSERT  "Failed INSERT on relation '%s'"

        /** "Trace" messages */
#define Er_Trace_SQL "%s %d: SQL command: %s  (%s)"
/*************************************************************/


/** ************************************************************
 * - How to write callback routines
 *     Callback (or "trigger") routines are available for read,
 * write, insert, and delete.
 *
 *     As mentioned above, read callbacks are executed before a
 * column value is used and write callbacks are called after all
 * columns have been updated.  Both read and write callbacks
 * return a zero on success and non-zero on failure.  Read
 * callbacks  have the following calling parameters:
 *   - char *tblname:  the name of the table referenced
 *   - char *colname:  the name of the column referenced
 *   - char *sqlcmd:   the text of the SQL command 
 *   - void *pr;       points to affected row in table
 *   - int   rowid:    the zero-indexed row number of the row
 *                     being read or written
 *
 *     Read callbacks are particularly useful to compute things
 * like sums and averages; things that aren't worth the effort
 * compute continuously if it's possible to compute it just 
 * when it is used.  The callback should return zero on success.
 * If a non-zero value is returned, the client gets an error
 * message, TRIGGERED ACTION EXCEPTION.
 *
 *     Write callbacks can form the real engine driving the
 * application.  These are most applicable when tied to 
 * configuration changes.   Write callbacks are also a useful 
 * place to log configuration changes.  Write callbacks have
 * the same parameters as read callbacks with the addition of
 * a pointer to a copy of the row before it was modified.
 * Access to a copy of the unmodified row is useful to detect
 * actual changes and not just updates with the same value.
 *     The parameters to a write callback are:
 *   - char *tblname:  the name of the table referenced
 *   - char *colname:  the name of the column referenced
 *   - char *sqlcmd:   the text of the SQL command 
 *   - void *pr;       points to affected row in table
 *   - int   rowid:    the zero-indexed row number of the row
 *                     being read or written
 *   - void *poldrow;  pointer to a copy of the row before any
 *                     changes were made.
 *
 * A write callback return value of zero indicates success.  A
 * non-zero value indicates an error.  On error, RTA will restore
 * the table row using the copy of the unmodified row and returns
 * an SQL error, TRIGGERED ACTION EXCEPTION, to the user.
 *
 *
 *     Insert and delete callbacks are invoked to add or remove
 * a row from a table. These routines are used (along with an
 * iterator) to manage tables in which the rows are allocated
 * dynamically and appear in a linked list or B-tree.
 *
 *     The RTA processing of INSERT allocates memory for the 
 * row itself as well as memory for any RTA pointer types in
 * the row.  Thus a row with a single integer pointer (RTA_PINT)
 * would have two memory allocations -- one for the PINT and one
 * for the row.
 *
 * Use an insert callback to attach the new row into the table
 * and to do application specific initialization of the row.
 * Write callbacks are called for the row after the insert callback
 * returns so you don't need to replicate that code in the insert
 * callback.
 *
 * If your insert callback detects a problem that values passed in
 * with the INSERT, have it return a negative value and RTA will
 * free the memory allocated for the new row and will return an
 * error to the user.  The text of the error is defined above as
 * the E_BADINSERT error.  If your insert callback succeeds, have
 * it return the zero-indexed row number of the new row in your
 * table.  This value is returned to the user as the OID.  That
 * is, the PostgreSQL response to an insert looks like:
 *   INSERT <OID> <#ROWS>
 * The number of rows inserted is alway one and the OID, while
 * normally the index of the new row, can actually be any non-negative
 * value you want.
 *
 * The parameters to a insert callback are:
 *   - char *tblname:  the name of the table referenced
 *   - char *sqlcmd:   the text of the SQL command 
 *   - void *pr;       points to allocated memory for the row
 *
 *     The delete callback is used to remove a row from a table.
 * This usually involves unlinking the row from a linked list and
 * freeing any memory allocated for the row.  If you are using
 * insert and delete be sure to >>> FREE THE MEMORY <<< of the
 * row.  Be sure to free all of the memory allocated for the
 * pointer types which were also allocated.
 *
 * The delete callback does not have a return value and its calling
 * parameters are the same as the insert callback:
 *   - char *tblname:  the name of the table referenced
 *   - char *sqlcmd:   the text of the SQL command 
 *   - void *pr;       points to allocated memory for the row
 *
 **************************************************************/

/***************************************************************
 * - Future enhancements:
 *    Several enhancements are possible for the librta package:
 * - printf format string in the column definition
 * - ability to register more than one trigger per column
 * - secure login maintaining Postgres compatibility
 * - IPC and support for shared tables in shared memory
 * - specify pre or post for the write callback
 * - table save callback (to save file to flash?)
 * - execution times for table access, update
 * - count(*) function
 * - model output buffer mgmt on zlib to allow output streams
 * - add a column data type of "table" to allow nested tables
 * - add internationalization support
 * - make it thread safe
 **************************************************************/

#endif
