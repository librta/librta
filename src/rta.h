/***************************************************************
 * Run Time Access
 * Copyright (C) 2003 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * rta.h  -- DB API for your internal structures and tables
 **************************************************************/

/** ************************************************************
 * - Preamble:
 * "rta" is a specialized memory resident data base interface.
 * It is not a stand-alone server but a library which attaches
 * to a program and offers up the program's internal structures
 * and arrays as data base tables.  It uses a subset of the 
 * Postgres protocol and is compatible with the Postgres bindings
 * for "C", PHP, and the Postgres command line tool, psql.
 *
 *    This file contains the defines, structures, and function
 * prototypes for the 'rta' package.
 *
 * INDEX: 
 *        - Preamble
 *        - Introduction and Purpose
 *        - Limits
 *        - Data Structures
 *        - Subroutines
 *        - rta UPDATE and SELECT syntax
 *        - Internal DB tables
 *        - List of all error messages
 *        - How to write callback routines
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
 *     The rta package helps solve these problems by giving
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
 *     The rta package allows any programming language with a 
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
 * tell rta about the tables and columns in the data base. 
 * Table information includes things like the name, start
 * address, number of rows and the length of each row.  Column
 * information includes things like the associate table name,
 * the column name, the column's data type, and whether we want
 * any special functions called when the column is read or
 * written (callbacks).
 *
 *   This document describes the API offered by the rta package.
 **************************************************************/

#ifndef RTA_H
#define RTA_H 1

/***************************************************************
 * - Limits:
 *     Here are the defines which describe the internal limits
 * set in the rta package.  You are welcome to change these 
 * limits; just be sure to recompile the rta package using 
 * your new settings.
 **************************************************************/

#include <limits.h>             /* for PATH_MAX */

        /** Maximum number of tables allowed in the system.
         * Your data base may not contain more than this number
         * of tables. */
#define MX_TBL        (500)

        /** Maximum number of columns allowed in the system.
         * Your data base may not contain more than this number
         * of columns. */
#define MX_COL       (2500)

        /** Maximum number of characters in a column name, table
         * name, and in help.  See TBLDEF and COLDEF below. */
#define MXCOLNAME      (30)
#define MXTBLNAME      (30)
#define MXHELPSTR    (1000)
#define MXFILENAME   PATH_MAX

        /** Maximum number of characters in the 'ident' field of
         * the openlog() call.  See the rta_dbg table below. */
#define MXDBGIDENT     (20)

        /** Maximum line size.  SQL commands in save files may
         * contain no more than MX_LN_SZ characters.  Lines with
         * more than MX_LN_SZ characters are silently truncated
         * to MX_LN_SZ characters. */
#define MX_LN_SZ     (1500)

    /* Maximum number of columns allowed in a table */
#define NCMDCOLS       (40)

/***************************************************************
 * - Data Structures:
 *     Each column and table in the data base must be described
 * in a data structure.  Here are the data structures and 
 * associated defines to describe tables and columns.
 **************************************************************/

          /** The column definition (COLDEF) structure describes
           * one column of a table.  A table description has an
           * array of COLDEFs to describe the columns in the
           * table. */
typedef struct
{

          /** The name of the table that has this column. */
  char    *table;

          /** The name of the column.  Must be at most MXCOLNAME
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
           * type is RTA_STR or RTA_PSTR. */
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
           * table name, the column name, the input SQL
           * command, and the (zero indexed) row number for the
           * row that is being read.
           * This routine is called *each* time the column is
           * read so the following would produce two calls:
           * SELECT intime FROM inns WHERE intime >= 100;   */
  void     (*readcb) (char *tbl, char *column, char *SQL, int row_num);

          /** Write callback.  This routine is called after an
           * UPDATE in which the column is written. Input values
           * include the table name, the column name, the SQL
           * command, and the (zero indexed) row number of the
           * modified row.  See the callback section below.
           * This routine is called only once after all column
           * updates have occurred.  For example, if there were
           * a write callback attached to the addr column, the
           * following SQL statement would cause the execution
           * of the write callback once after both mask and addr
           * have been written:
           * UPDATE ethers SET mask="255.255.255.0", addr = \
           *     "192.168.1.10" WHERE name = "eth1";
           * The callback is called for each row modified. */
  void     (*writecb) (char *tbl, char *column, char *SQL, int row_num);

          /** A brief description of the column.  This should
           * include the meaning of the data in the column, the
           * limits, if any, and the default values.  Include
           * a brief description of the side effects of changes.
           * This field is particularly important for tables 
           * which are part of the "boundary" between the UI
           * developers and the application programmers.  */
  char    *help;
}
COLDEF;

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
         * long long.  On Linux/gcc/Pentium a long long is 64
         * bits.  */
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
#define MXCOLTYPE       (RTA_PFLOAT)

        /** The boolean flags.  
         * If the disksave bit is set any writes to the column
         * causes the table to be saved to the "savefile".  See
         * savefile described in the TBLDEF section below. */
#define RTA_DISKSAVE     (1<<0)

        /** If the readonly flag is set, any writes to the 
         * column will fail and a debug log message will be 
         * sent.  (For unit test you may find it very handy to
         * leave this bit clear to get better test coverage of
         * the corner cases.)   */
#define RTA_READONLY     (1<<1)

        /** The table definition (TBLDEF) structure describes
         * a table and is passed into the DB system by the
         * rta_add_table() subroutine.  */
typedef struct
{

        /** The name of the table.  Must be less than than
         * MXTLBNAME characters in length.  Must be unique
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

        /** An array of COLDEF structures which describe each
         * column in the table.  These must be in statically
         * allocated memory since the rta system references
         * them while running.  */
  COLDEF  *cols;

        /** The number of columns in the table.  That is, the
         * number of COLDEFs defined by 'cols'.  */
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
TBLDEF;

/***************************************************************
 * - Subroutines
 * Here is a summary of the few routines in the rta API:
 *    rta_init()      - initialize internal tables
 *    dbcommand()     - I/F to Postgres clients
 *    egp_add_table() - add a table and its columns to the DB
 *    SQL_string()    - execute an SQL statement in the DB
 *    rta_save()      - save a table to a file
 *    rta_load()      - load a table from a file
 *
 * The FUSE based virtual filesystem adds the following:
 *    rtafs_init()    - mount FS and get file descriptor for it
 *    do_rtafs()      - handle all virtual file system IO
 * The above two routines are in the rtafs library.  Their 
 * inclusion in this include file cause no harm if you are
 * using only the rtadb library.  Note that the rtafs library
 * requires the rtadb library.
 **************************************************************/

/** ************************************************************
 * dbcommand():  - Depacketize and execute Postgres commands.
 *
 * The main application accepts TCP connections from Postgres
 * clients and passes the stream of bytes (encoded SQL requests)
 * from the client into the rta system via this routine.  If the
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
int      dbcommand(char *, int *, char *, int *);

/** ************************************************************
 * rta_add_table():  - Register a table for inclusion in the
 * DB interface.  Adding a table allows external Postgres
 * clients access to the table's content.
 *     Note that the TBLDEF structure must be statically
 * allocated.  The DB system keeps just the pointer to the table
 * and does not copy the information.  This means that you can
 * change the contents of the table definition by changing the
 * contents of the TBLDEF structure.  This might be useful if
 * you need to allocate more memory for the table and change its
 * row count and address.
 *    An error is returned if another table by the same name 
 * already exists in the DB or if the table is defined without
 * any columns.
 *    If a 'savefile' is specified, it is loaded.  (See the
 * rta_load() command below for more details.)
 * 
 * Input:  ptbl          - pointer to the TBLDEF to add
 * Return: RTA_SUCCESS   - table added
 *         RTA_ERROR     - error
 **************************************************************/
int      rta_add_table(TBLDEF *);

/** ************************************************************
 * rta_init():  - Initialize all internal variables and tables
 * This may be called more than once.
 * 
 * Input:  None
 * Return: None
 **************************************************************/
void     rta_init();

/** ************************************************************
 * SQL_string():  - Execute single SQL command
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
 *         out - the buffer to hold responses back to client,
 *         nout - on entry, the number of free bytes in 'out'
 *               on exit, the number of remaining free bytes
 * Return: 
 **************************************************************/
void     SQL_string(char *, char *, int *);

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
 * Input:  ptbl   - pointer to the TBLDEF structure for the 
 *                  table to save
 *         fname  - null terminated string with the path and
 *                  file name for the stored data.
 * Return: RTA_SUCCESS   - table saved
 *         RTA_ERROR     - some kind of error
 **************************************************************/
int      rta_save(TBLDEF *, char *);

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
int      rta_load(TBLDEF *, char *);

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
 * rtafs_init():  - Initialize the virtual file system interface
 * to RTA.  The single input parameter is the mount point for
 * the VFS.  On success, the return value is a file descriptor
 * to the VFS.  On failure, a -1 is returned and errno is set.
 * This file descriptor should be used in subsequent select() or
 * poll() calls to notify your program of file system activity.
 * An important SIDE EFFECT is that the signal handlers for
 * SIGHUP, SIGINT, and SIGTERM are set.  The signal handler
 * tries to unmount the virtual file system.  This routine
 * is part of the librtafs library.
 *   Note that FUSE requires that the owner and group of the
 * mount point be the same as the owner and group of the program
 * that does the mount.  For example, if your mount point is
 * owned by Apache, then your the UID of your program must be
 * Apache as well.
 *
 * Input:  char *mountpoint - desired mount point
 *
 * Return: int fd        - file descriptor on success.
 **************************************************************/
int      rtafs_init(char *);

/** ************************************************************
 * do_rtafs():  - Handle all actual virtual file system IO. 
 * This routine handles all file system IO for the virtual file
 * system mounted by the rtafs_init() call.  This routine should
 * be called when there is activity on the file descriptor
 * returned from rtafs_init().  It has no input or output
 * parameters.  This routine is part of the librtafs library.
 * 
 * Input:  (none)
 *
 * Return: (none)
 **************************************************************/
void     do_rtafs();

/** ************************************************************
 * - rta UPDATE and SELECT syntax
 *     rta IS AN API, *NOT* A DATABASE!
 * Neither the rta UPDATE nor the rta SELECT adhere to the
 * Postgres equivalents.  Joins are not allowed, and the WHERE
 * clause supports only the AND relation.  There are no locks
 * or transactions.
 *
 * SELECT:
 *    SELECT column_list FROM table [where_clause] [limit_clause]
 *
 *    SELECT supports multiple columns, '*', LIMIT, and OFFSET.
 * At most MXCMDCOLS columns can be specified in the select list
 * or in the WHERE clause.  LIMIT restricts the number of rows
 * returned to the number specified.  OFFSET skips the number of
 * rows specified and begins output with the next row.
 * 'column_list' is a '*' or 'column_name [, column_name ...]'.
 * 'where_clause' is 'col_name = value [AND col_name = value ..]'
 * in which all col=val pairs must match for a row to match.
 *     LIMIT and OFFSET are very useful to prevent a buffer
 * overflow on the output buffer of dbcommand().  They are also
 * very useful for web based user interfaces in which viewing
 * the data a page-at-a-time is desirable.
 *     Column and table names are case sensitive.  If a column
 * or table name is one of the reserved words it must be placed
 * in quotes when used.  The reserved words are: AND, FROM, 
 * LIMIT, OFFSET, SELECT, SET, UPDATE, and WHERE.  Reserved 
 * words are *not* case sensitive.  You may use lower case
 * reserved words in your 
 *    Comparison operator in the WHERE clause include =, >=,
 * <=, >, and <.
 *    You can use a reserved word, like OFFSET, as a column name
 * but you will need to quote it whenever you reference it in an
 * SQL command (SELECT "offset" FROM tunings ...).   Strings 
 * may contain any of the !@#$%^&*()_+-={}[]\|:;<>?,./~`
 * characters.  If a string contains a double quote, use a 
 * single quote to wrap it (eg 'The sign say "Hi mom!"'), and
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
 * - Internal DB tables
 *     rta has four tables visible to the application:
 *  rta_tables:      - a table of all tables in the DB
 *  rta_columns:     - a table of all columns in the DB
 *  rta_logconfig:   - controls what gets logged from rta
 *  egp_stats:       - simple usage and error statistics
 *
 *     The rta_tables table gives SQL access to all internal and
 * registered tables.  The data in the table is exactly that of
 * the TBLDEF structures registered with rta_add_table().  This
 * table is used for the generic table viewer and table editor 
 * applications used mostly for application debugging.  The
 * columns of rta_tables are:
 *     name      - the name of the table
 *     address   - the start address of the table in memory
 *     rowlen    - number of bytes in each row of the table
 *     nrows     - number of rows in the table
 *     cols      - pointer to array of column definitions
 *     ncol      - number of columns in the table
 *     savefile  - the file used to store non-volatile columns
 *     help      - a description of the table
 *
 *     The rta_columns table has the column definitions of all
 * columns in the DB.  The data in the table is exactly that of
 * the COLDEF structures registered with rta_add_table().  This
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
 * exact mapping.  The rta package generates no user level log
 * messages, only debug messages.  All of the fields in this 
 * table are volatile.  You will need to set the values in your
 * main program to make them seem persistent.  (Try something
 * like "SQL_string("UPDATE rta_dbgconfig SET dbg ....").)
 * The columns of rta_dbgconfig are:
 *     syserr - integer, 0 means no log, 1 means log.
 *              This logs OS call errors like malloc()
 *              failures.  Default is 1.
 *     rtaerr - integer, 0 means no log, 1 means log.
 *              Enables logging of errors internal to the
 *              rta package itself.  Default is 1.
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
 *              the priority to use when sending rta debug
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
 *              the facility to use when sending rta debug
 *              messages.  It is best to use the defines in
 *              .../sys/syslog.h to set this.  The default
 *              is LOG_USER.  Changes to this do not take
 *              effect until dbg_target is updated.
 *     ident  - string.  Syslog() requires an 'ident' string as
 *              part of all log messages.  This specifies
 *              the ident string to use when sending rta debug
 *              messages.  This is normally set to the process
 *              or command name.  The default is "rta".  Changes
 *              to this do not take effect until dbg_target
 *              is updated.  This can be at most MXDBGIDENT
 *              characters in length.
 *
 *     The rta_stat table contains usage and error statistics
 * which might be of interest to developers.  All fields are
 * of type long, are read-only, and are set to zero by
 * rta_init().  The columns of rta_stats are:
 *     nsyserr    - count of failed OS calls. 
 *     nrtaerr    - count of internal rta failures.
 *     nsqlerr    - count of SQL failures.
 *     nauth      - count of authorizations. (==#connections)
 *     nupdate    - count of UPDATE or file write requests
 *     nselect    - count of SELECT or file read requests
 *
 **************************************************************/

/** ************************************************************
 * - List of all messages
 *      There are two types of error messages available in the
 * rta package.  The first type is the error messages returned
 * as part of an SQL request.  The messages of this type are:
 *
 * 1) "ERROR:  Relation '%s' does not exist"
 *      This reply indicates that a table requested in a SELECT
 *      UPDATE, or where clause does not exist.  The %s is 
 *      replaced by the name of the requested table.
 * 2) "ERROR:  Attribute '%s' not found\n"
 *      This reply indicates that a column requested in a SELECT
 *      UPDATE, or where clause does not exist.  The %s is 
 *      replaced by the name of the requested column.
 * 3) "ERROR:  SQL parse error"
 *      This reply indicates a mal-formed SQL request or a
 *      mis-match in the types of data in a where clause or in 
 *      an update list.
 * 4) "ERROR:  Output buffer full"
 *      This reply indicates that the size of the response to
 *      a request exceeds the size of the output buffer.  See
 *      dbcommand() and the 'out' and 'nout' parameters.  This
 *      error can be avoided with a large enough output buffer
 *      or, preferably, with the use of LIMIT and OFFSET.
 * 5) "ERROR:  String too long for '%s'
 *      This reply indicates that an update to a column of type
 *      string or pointer to string would have exceeded the
 *      width of the column.  The %s is replaced by the column
 *      name.
 * 6) "ERROR:  Can not update read-only column '%s'
 *      This reply indicates that an attempt to update a column
 *      marked as read-only.  The %s is replaced by the column
 *      name.
 *
 *     The other type of error messages are internal debug
 * messages.  Debug messages are logged using the standard
 * syslog() facility available on all Linux systems.  The
 * default syslog "facility" used is LOG_USER but this can be
 * changed by setting 'facility' in the rta_dbg table.
 *     You are welcome to modify syslogd in order to do post
 * processing such as generating SNMP traps off these debug
 * messages.  All error messages of this type are send to
 * syslog() as:    "egp[PID]: FILE LINE#: error_message",
 * where PID, FILE, and LINE# are replaced by the process ID,
 * the source file name, and the line number where the error
 * was detected.
 *     Following are the defines used to generate these debug
 * and error messages.   The "%s %d" at the start of each 
 * error string is replaced by the file name and line number
 * where the error is detected.  */

        /** "System" errors */
#define Er_No_Mem       "%s %d: Can not allocate memory\n"
#define Er_No_Save      "%s %d: Table save failure.  Can not open %s\n"
#define Er_No_Load      "%s %d: Table load failure.  Can not open %s\n"

        /** "RTA" errors */
#define Er_Max_Tbls     "%s %d: Too many tables in DB\n"
#define Er_Max_Cols     "%s %d: Too many columns in DB\n"
#define Er_Tname_Big    "%s %d: Too many characters in table name: %s\n"
#define Er_Cname_Big    "%s %d: Too many characters in column name: %s\n"
#define Er_Hname_Big    "%s %d: Too many characters in help text: %s\n"
#define Er_Tbl_Dup      "%s %d: DB already has table named: %s\n"
#define Er_Col_Dup      "%s %d: Table already has column named: %s\n"
#define Er_Col_Type     "%s %d: Column contains an unknown data type: %s\n"
#define Er_Col_Flag     "%s %d: Column contains unknown flag data: %s\n"
#define Er_Cmd_Cols     "%s %d: Too many columns in table: %s\n"
#define Er_No_Space     "%s %d: Not enough buffer space\n"

        /** "SQL" errors */
#define Er_Bad_SQL      "%s %d: SQL parse error: %s\n"
#define Er_Readonly     "%s %d: Attempt to update readonly column: %s\n"

        /** "Trace" messages */
#define Er_Trace_SQL    "%s %d: SQL command: %s  (%s)\n"

/*************************************************************/

/** ************************************************************
 * - How to write callback routines
 *     As mentioned above, read callbacks are executed before a
 * column value is used and write callbacks are called after all
 * columns have been updated.  Both read and write callbacks
 * return nothing (are of type void) and have the same calling
 * parameters:
 *   - char *tblname:  the name of the table referenced
 *   - char *colname:  the name of the column referenced
 *   - char *sqlcmd:   the text of the SQL command 
 *   - int   rowid:    the zero-indexed row number of the row
 *                     being read or written
 *
 *     Read callbacks are particularly useful to compute things
 * like sums and averages; things that aren't worth the effort
 * compute continuously if it's possible to compute it just 
 * when it is used.
 *     Write callbacks can form the real engine driving the
 * application.  These are most applicable when tied to 
 * configuration changes.   Write callbacks are also a useful 
 * place to log configuration changes.
 **************************************************************/

/***************************************************************
 * - Future enhancements:
 *    Several enhancements are possible for the rta package:
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
 **************************************************************/

#endif
