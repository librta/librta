/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * api.c  -- routines to provide a PostgreSQL DB API to
 * embedded systems.
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>             /* for mkstemp() */
#include <libgen.h>             /* for dirname() */
#include <string.h>             /* for strlen() */
#include <limits.h>             /* for PATH_MAX */
#include <syslog.h>
#include <sys/types.h>          /* for stat() */
#include <sys/stat.h>           /* for stat() */
#include <unistd.h>             /* for stat() */
#include "rta.h"                /* for various constants */
#include "do_sql.h"             /* for LOC */

/* rta_Tbl and rta_Col contain pointers to table and column
 * definitions of all tables and columns in * the system.
 * rta_Ntbl and rta_Ncol are the number of tables and columns in each
 * list.  These are used often enough that they are globals.
 * rta_Ntbl starts out with a -1 flag to indicate that we are not
 * initialized.  */
 
RTA_TBLDEF  *rta_Tbl[RTA_MX_TBL];
int          rta_Ntbl = -1;
RTA_COLDEF  *rta_Col[RTA_MX_COL];
int          rta_Ncol;

extern struct RtaDbg rta_dbg;
static char  *ConfigDir = (char *) 0;

static int is_reserved(char *pword);


/***************************************************************
 * rta_init(): - Initialize all internal system tables.
 *
 * Input:        None.
 * Output:       None.
 **************************************************************/
void
rta_init()
{
  int      i;          /* loop index */
  extern RTA_TBLDEF rta_tablesTable;
  extern RTA_TBLDEF rta_columnsTable;
  extern RTA_TBLDEF rta_dbgTable;
  extern RTA_TBLDEF rta_statTable;
  extern void rta_restart_syslog();

  for (i = 0; i < RTA_MX_TBL; i++) {
    rta_Tbl[i] = (RTA_TBLDEF *) 0;
  }
  rta_Ntbl = 0;

  /* add system and internal tables here */
  (void) rta_add_table(&rta_tablesTable);
  (void) rta_add_table(&rta_columnsTable);
  (void) rta_add_table(&rta_dbgTable);
  (void) rta_add_table(&rta_statTable);

  rta_restart_syslog((char *) 0, (char *) 0, (char *) 0, (void *) 0,
		     (void *) 0, 0);
}



/***************************************************************
 * rta_config_dir(): - Set the default directory for all RTA
 * savefiles.  The directory specified here is prepended to
 * all file names before a load of the .sql file is attempted.
 *
 * Input:        char *  -- the config directory or path
 * Output:       0 if the dir seems OK, -1 otherwise
 **************************************************************/
int
rta_config_dir(char *configdir)
{
  struct stat statbuf;      /* to verify input is a directory */
  int         len;          /* length of the path */

  /* Initialize the RTA tables if this is the first call to add_table */
  if (rta_Ntbl == -1)
    rta_init();

  /* Perform some sanity checks */
  if (!stat(configdir, &statbuf) && S_ISDIR(statbuf.st_mode)) {
    ConfigDir = strdup(configdir);
    if (ConfigDir) {
      len = strlen(ConfigDir);
      if (len != 1 && ConfigDir[len -1] == '/') {
        ConfigDir[len - 1] = (char) 0;
      }
      return(0);
    }
  }
  return(-1);
}

/***************************************************************
 * rta_add_table(): - Add one table to the list of
 * tables in the system.  If the table has an associated
 * "savefile" we try to open the savefile and execute any SQL
 * commands found there.
 *
 * Input:  ptbl:  pointer to the table to add
 * Output: RTA_SUCCESS   - Add successful
 *         RTA_DUP       - Table is already in the list.  (Note
 *                         that this might not be an error since
 *                         we can allow redefinition of a table)
 *         RTA_ERROR     - The passed table definition has a
 *                         problem which prevents its addition.
 *                         A syslog error message describes the
 *                         problem
 **************************************************************/
int
rta_add_table(RTA_TBLDEF *ptbl)
{
  extern struct RtaStat rta_stat;
  extern RTA_TBLDEF rta_columnsTable;
  int      i, j;       /* a loop index */


  /* Initialize the RTA tables if this is the first call to add_table */
  if (rta_Ntbl == -1)
    rta_init();

  /* Error if at rta_Ntbl limit */
  if (rta_Ntbl == RTA_MX_TBL) {
    rta_stat.nrtaerr++;
    if (rta_dbg.rtaerr)
      rta_log(LOC, Er_Max_Tbls);
    return (RTA_ERROR);
  }

  /* verify that table name is unique */
  i = 0;
  while (i < rta_Ntbl) {
    if (!strncmp(ptbl->name, rta_Tbl[i]->name, RTA_MXTBLNAME)) {
      rta_stat.nrtaerr++;
      if (rta_dbg.rtaerr)
        rta_log(LOC, Er_Tbl_Dup, ptbl->name);
      return (RTA_ERROR);
    }
    i++;
  }

  /* verify length of table name */
  if (strlen(ptbl->name) > RTA_MXTBLNAME) {
    rta_stat.nrtaerr++;
    if (rta_dbg.rtaerr)
      rta_log(LOC, Er_Tname_Big, ptbl->name);
    return (RTA_ERROR);
  }

  /* verify table name is not a reserved word */
  if (is_reserved(ptbl->name)) {
    rta_stat.nrtaerr++;
    if (rta_dbg.rtaerr)
      rta_log(LOC, Er_Reserved, ptbl->name);
    return (RTA_ERROR);
  }

  /* verify savefile name is a valid pointer */
  if (ptbl->savefile == (char *) 0) {
    rta_stat.nrtaerr++;
    if (rta_dbg.rtaerr)
      rta_log(LOC, Er_Col_Type, "savefile");
    return (RTA_ERROR);
  }

  /* Check the upper bound on # columns / table */
  if (ptbl->ncol > RTA_NCMDCOLS) {
    rta_stat.nrtaerr++;
    if (rta_dbg.rtaerr)
      rta_log(LOC, Er_Cmd_Cols, ptbl->name);
    return (RTA_ERROR);
  }

  /* verify that column names are unique within table */
  for (i = 0; i < ptbl->ncol; i++) {
    for (j = 0; j < i; j++) {
      if (!strncmp(ptbl->cols[i].name, ptbl->cols[j].name, RTA_MXCOLNAME)) {
        rta_stat.nrtaerr++;
        if (rta_dbg.rtaerr)
          rta_log(LOC, Er_Col_Dup, ptbl->name, ptbl->cols[i].name);
        return (RTA_ERROR);
      }
    }
  }

  /* verify column name length, help length, data type, flag contents,
     and that column table name is valid */
  for (i = 0; i < ptbl->ncol; i++) {
    if (strlen(ptbl->cols[i].name) > RTA_MXCOLNAME) {
      rta_stat.nrtaerr++;
      if (rta_dbg.rtaerr)
        rta_log(LOC, Er_Cname_Big, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
    if (is_reserved(ptbl->cols[i].name)) {
      rta_stat.nrtaerr++;
      if (rta_dbg.rtaerr)
        rta_log(LOC, Er_Reserved, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
    if (strlen(ptbl->cols[i].help) > RTA_MXHELPSTR) {
      rta_stat.nrtaerr++;
      if (rta_dbg.rtaerr)
        rta_log(LOC, Er_Hname_Big, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
    if (ptbl->cols[i].type > RTA_MXCOLTYPE) {
      rta_stat.nrtaerr++;
      if (rta_dbg.rtaerr)
        rta_log(LOC, Er_Col_Type, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
    if (ptbl->cols[i].flags > RTA_DISKSAVE + RTA_READONLY) {
      rta_stat.nrtaerr++;
      if (rta_dbg.rtaerr)
        rta_log(LOC, Er_Col_Flag, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
    if (strcmp(ptbl->cols[i].table, ptbl->name)) {
      rta_stat.nrtaerr++;
      if (rta_dbg.rtaerr)
        rta_log(LOC, Er_Col_Name, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
  }

  /* Verify that we can add the columns */
  if ((rta_Ncol + ptbl->ncol) >= RTA_MX_COL) {
    rta_stat.nrtaerr++;
    if (rta_dbg.rtaerr)
      rta_log(LOC, Er_Max_Cols);
    return (RTA_ERROR);
  }

  /* Everything looks OK.  Add table and columns */
  rta_Tbl[rta_Ntbl++] = ptbl;
  rta_Tbl[0]->nrows = rta_Ntbl;

  /* Add columns to list of column pointers */
  for (i = 0; i < ptbl->ncol; i++) {
    rta_Col[rta_Ncol++] = &(ptbl->cols[i]);
  }
  rta_columnsTable.nrows += ptbl->ncol;

  /* Execute commands in the save file to restore */
  if (ptbl->savefile && strlen(ptbl->savefile) > 0)
    (void) rta_load(ptbl, ptbl->savefile);

  return (RTA_SUCCESS);
}

/***************************************************************
 * PostgreSQL "packets" are identified by their first few bytes.
 * The protocol uses the first byte to identify the packet type.  I
 * If the first byte is a zero the packet is either a start-up
 * packet or a cancel packet.  If the first byte is not a zero
 * the packet is a command packet.  Most packets have the command
 * byte followed by four bytes of packet length.  Note that
 * multi-byte data is sent with the most significant byte first.
 * Please see the full documentation in the "PostgreSQL 7.4
 * Developer's Guide" at http://www.postgresql.org/
 * 
 **************************************************************/

/***************************************************************
 * rta_dbcommand():  - Depacketize and execute any Postgres 
 * commands in the input buffer.  
 * 
 * Input:  buf - the buffer with the Postgres packet
 *         nin - on entry, the number of bytes in 'buf',
 *               on exit, the number of bytes remaining in buf
 *         out - the buffer to hold responses back to client
 *         nout - on entry, the number of free bytes in 'out'
 *               on exit, the number of remaining free bytes
 * Return: RTA_SUCCESS   - executed one command
 *         RTA_NOCMD     - input did not have a full cmd
 *         RTA_ERROR     - some kind of error 
 *         RTA_CLOSE     - client requests a orderly close
 *         RTA_NOBUF     - insufficient output buffer space
 **************************************************************/
int
rta_dbcommand(char *buf, int *nin, char *out, int *nout)
{
  extern struct RtaStat rta_stat;
  int      length;     /* length of the packet if old protocol */

  /* startup or cancel packet if first byte is zero */
  if ((int) buf[0] == 0) {
    /* get length.  Enough bytes for a length? if not, consume no
       input, write no output */
    if (*nin < 4) {
      return (RTA_NOCMD);
    }
    length = (int) ((unsigned int) (0xff & buf[3]) +
      ((unsigned int) (0xff & buf[2]) << 8) +
      ((unsigned int) (0xff & buf[1]) << 16));

    /* Is the whole packet here? If not, consume no input, write no
       output */
    if (*nin < length) {
      return (RTA_NOCMD);
    }

    /* The first packet can be a request for an SSL connection.
       Look for this and return an 'N' to indicate that we do not
       support SSL.  The packet is '00 00 00 08 04 d2 16 2f'.  Note
       that '04 d2' and '16 2f' are 1234 and 5678 respectively. */
    if (length == 8 &&
        buf[4] == (char) 0x04 && buf[5] == (char) 0xd2 &&
        buf[6] == (char) 0x16 && buf[7] == (char) 0x2f) {
      *out = 'N';
      out++;
      *nout -= 1;
      *nin -= length;
      return (RTA_SUCCESS);
    }

    /* Look for a start-up request packet.  Do a sanity check since the 
       minimum startup packet is 13 bytes (4 length, 4 protocol,
       'user', and a null). */
    if (length < 13) {          /* unknown, ignore, (log?) */
      *nin -= length;
      return (RTA_SUCCESS);
    }

    /* A start-up packet if 0300 in the protocol field */
    if (buf[4] == 0 && buf[5] == 3 && buf[6] == 0 && buf[7] == 0) {
      /* Currently we do not authenticate the user. If you want to
         add real authentication, this is the place to add it.  */

      /* *INDENT-OFF* */
      /* This is the canned response we send on all start-ups.
         See the Postgres protocol specification for details.
         'R', int32(length), int32(0)
         'S', int32(length), "client_encoding", null, "SQL_ASCII", null
         'S', int32(length), "DataStyle", null, "ISO, MDY", null
         'S', int32(length), "is_superuser", null, "on", null
         'S', int32(length), "server_version", "7.4", null
         'S', int32(length), "session_authorization", null, "postgres", null
         'K', int32(length), int32(pid of backend), int32(secret key)
         'Z', int32(length), 'I'   */

      unsigned char reply[164] = {
        'R',0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,
        'S',0x00,0x00,0x00,0x1e,0x63,0x6c,0x69,0x65,0x6e,0x74,0x5f,0x65,
            0x6e,0x63,0x6f,0x64,0x69,0x6e,0x67,0x00,0x53,0x51,0x4c,0x5f,
            0x41,0x53,0x43,0x49,0x49,0x00,
        'S',0x00,0x00,0x00,0x17,0x44,0x61,0x74,0x65,0x53,0x74,0x79,0x6c,
            0x65,0x00,0x49,0x53,0x4f,0x2c,0x20,0x4d,0x44,0x59, 0x00,
        'S',0x00,0x00,0x00,0x14,0x69,0x73,0x5f,0x73,0x75,0x70,0x65,0x72,
            0x75,0x73,0x65,0x72,0x00,0x6f,0x6e,0x00,
        'S',0x00,0x00,0x00,0x17,0x73,0x65,0x72,0x76,0x65,0x72,0x5f,0x76,
            0x65,0x72,0x73,0x69,0x6f,0x6e,0x00,0x37,0x2e,0x34,0x00,
        'S',0x00,0x00,0x00,0x23,0x73,0x65,0x73,0x73,0x69,0x6f,0x6e,0x5f,
            0x61,0x75,0x74,0x68,0x6f,0x72,0x69,0x7a,0x61,0x74,0x69,0x6f,
            0x6e,0x00,0x70,0x6f,0x73,0x74,0x67,0x72,0x65,0x73,0x00,
        'K',0x00,0x00,0x00,0x0c,0x00,0x00,0x36,0x94,0x56,0xf4,0x8d,0x68,
        'Z',0x00,0x00,0x00,0x05,0x49};
      /* *INDENT-ON* */

      /* Verify that the buffer has enough room for the response */
      if (*nout < 164) {
        rta_stat.nsqlerr++;
        if (rta_dbg.sqlerr)
          rta_log(LOC, Er_No_Space);

        return (RTA_NOBUF);
      }

      *nin -= length;
      (void) memcpy(out, reply, 164);
      *nout -= 164;
      rta_stat.nauth++;
      return (RTA_SUCCESS);
    }
    else if (length == 16) {    /* a cancel request */
      /* ignore the request for now */
      *nin -= length;
      return (RTA_SUCCESS);
    }
    else {                      /* unknown, ignore, (log?) */

      *nin -= length;
      return (RTA_SUCCESS);
    }
  }
  else if (buf[0] == 'Q') {     /* a query request */
    /* the Postgres 0300 protocol has a 32 bit length after the 1 byte
       command.  Verify that we have enough bytes to get the length */

    if (*nin < 5) {
      return (RTA_NOCMD);
    }

    /* Get length and verify that we have all the bytes. Note the quiet 
       assumption that the length is 24 bits. */
    length = (int) ((unsigned int) (0xff & buf[4]) +
      ((unsigned int) (0xff & buf[3]) << 8) +
      ((unsigned int) (0xff & buf[2]) << 16));

    /* add one to account for the 'Q' */
    length++;
    if (*nin < length) {
      return (RTA_NOCMD);
    }

    /* Got a complete command; do it. (buf[5] since the SQL follows the 
       'Q' and length) */
    rta_SQL_string(&buf[5], (*nin - 5), out, nout);
    *nin -= length;             /* to swallow the cmd */
    return (RTA_SUCCESS);
  }
  else if (buf[0] == 'X') {     /* a terminate request */
    return (RTA_CLOSE);
  }

  /* an unknown request (should be logged?) */
  return (RTA_CLOSE);
}

/***************************************************************
 * rta_save():  - Save a table to file.  The save format is a
 * series of UPDATE commands saved in the file specified.  The
 * file is typically read in later and executed one line at a
 * time.
 * 
 * Input:  ptbl - pointer to the table to be saved
 *         fname - string with name of the save file
 *
 * Return: RTA_SUCCESS   - table saved
 *         RTA_ERROR     - some kind of error
 **************************************************************/
int
rta_save(RTA_TBLDEF *ptbl, char *fname)
{
  extern struct RtaStat rta_stat;
  int      sr;         /* the Size of each Row in the table */
  int      rx;         /* Row indeX */
  void    *pr;         /* Pointer to the row in the table/column */
  void    *pd;         /* Pointer to the Data in the table/column */
  int      cx;         /* Column index while building Data pkt */
  char     tfile[PATH_MAX];
  char     path[PATH_MAX];  /* full path/file name */
  int      fd;         /* file descriptor of temp file */
  FILE    *ftmp;       /* FILE handle to the temp file */
  int      did_header; /* == 1 if printed UPDATE part */
  int      did_1_col;  /* == 1 if at least one col printed */
  int      hcx;        /* header column index while building INSERT */
  int      did_1_hcol; /* == 1 if at least one INSERT col printed */
  

  /* Fill in the path with the full path to the config file */
  path[0] = (char) 0;
  if (fname[0] == '/') {
    (void) strncpy(path, fname, PATH_MAX);
    path[PATH_MAX-1] = (char) 0;
  }
  else {
    if (ConfigDir) {
      (void) strcpy(path, ConfigDir);
      strcat(path, "/");
    }
    strcat(path, fname);
  }

  /* Do a sanity check on the lengths of the paths involved */
  if (strlen(path) + strlen("/tmpXXXXXX") > PATH_MAX -1) {
    rta_stat.nsyserr++;
    if (rta_dbg.syserr)
      rta_log(LOC, Er_No_Save, ptbl->name, path);
    return (RTA_ERROR);
  }

  /* Open a temp file in the same directory as the users target file */
  (void) strcpy(tfile, path);
  (void) dirname(tfile);
  (void) strcat(tfile, "/tmpXXXXXX");
  fd = mkstemp(tfile);
  if (fd < 0) {
    rta_stat.nsyserr++;
    if (rta_dbg.syserr)
      rta_log(LOC, Er_No_Save, ptbl->name, tfile);
    return (RTA_ERROR);
  }
  ftmp = fdopen(fd, "w");
  if (ftmp == (FILE *) 0) {
    rta_stat.nsyserr++;
    if (rta_dbg.syserr)
      rta_log(LOC, Er_No_Save, ptbl->name, tfile);
    return (RTA_ERROR);
  }

  /* OK, temp file is open and ready to receive table data.
   * What gets put into the savefile depends on whether or 
   * not the table uses INSERT.  If it does, then we save the
   * table as a series of INSERTs.  If it does not, then we
   * save the table as a series of UPDATEs.  What makes this
   * messy is that an INSERT requires two passes through the
   * columns and an UPDATE requires one.  That is, ....
   * INSERT INTO tbl (col1, col2, ...) VALUES (val1, val2, ...)
   * UPDATE tbl SET col1=val1, col2=val2, ...    */

  /* Get row length and a pointer to the first row */
  sr = ptbl->rowlen;
  rx = 0;
  if (ptbl->iterator)
    pr = (ptbl->iterator) ((void *) NULL, ptbl->it_info, rx);
  else
    pr = ptbl->address;

  /* for each row ..... */
  while (pr) {
    did_header = 0;
    did_1_col = 0;
    did_1_hcol = 0;
    for (cx = 0; cx < ptbl->ncol; cx++) {
      if ((!(ptbl->cols[cx].flags & RTA_DISKSAVE)) ||
        (ptbl->cols[cx].flags & RTA_READONLY))
        continue;
      if (!did_header) {
        if (!ptbl->insertcb)
          fprintf(ftmp, "UPDATE %s SET ", ptbl->name);
        else {
          /* the header for insert is little more complex */
          fprintf(ftmp, "INSERT INTO %s (", ptbl->name);
          for (hcx = 0; hcx < ptbl->ncol; hcx++) {
            if ((!(ptbl->cols[hcx].flags & RTA_DISKSAVE)) ||
              (ptbl->cols[hcx].flags & RTA_READONLY))
              continue;
            if (did_1_hcol)
              fprintf(ftmp, ", %s", ptbl->cols[hcx].name);
            else {
              did_1_hcol = 1;
              fprintf(ftmp, "%s", ptbl->cols[hcx].name);
            }
          }
          fprintf(ftmp, ") VALUES (");
        }
        did_header = 1;
      }
      if (did_1_col)
        fprintf(ftmp, ", ");
      if (!ptbl->insertcb)
        fprintf(ftmp, "%s = ", ptbl->cols[cx].name);

      /* compute pointer to actual data */
      pd = (char *)pr + ptbl->cols[cx].offset;
      switch ((ptbl->cols[cx]).type) {
        case RTA_STR:
          if (memchr((char *) pd, '"', ptbl->cols[cx].length))
            fprintf(ftmp, "\'%s\'", (char *) pd);
          else
            fprintf(ftmp, "\"%s\"", (char *) pd);
          break;
        case RTA_PSTR:
          if (memchr((char *) pd, '"', ptbl->cols[cx].length))
            fprintf(ftmp, "\'%s\'", *(char **) pd);
          else
            fprintf(ftmp, "\"%s\"", *(char **) pd);
          break;
        case RTA_INT:
          fprintf(ftmp, "%d", *((int *) pd));
          break;
        case RTA_PINT:
          fprintf(ftmp, "%d", **((int **) pd));
          break;
        case RTA_LONG:
          fprintf(ftmp, "%lld", *((llong *) pd));
          break;
        case RTA_PLONG:
          fprintf(ftmp, "%lld", **((llong **) pd));
          break;
        case RTA_PTR:

          /* works only if INT and PTR are same size */
          fprintf(ftmp, "%d", *((int *) pd));
          break;
        case RTA_FLOAT:
          fprintf(ftmp, "%20.10f", *((float *) pd));
          break;
        case RTA_PFLOAT:
          fprintf(ftmp, "%20.10f", **((float **) pd));
          break;

        case RTA_SHORT:
          fprintf(ftmp, "%d", *((short *) pd));
          break;
        case RTA_UCHAR:
          fprintf(ftmp, "%d", *((unsigned char *) pd));
          break;
        case RTA_DOUBLE:
          fprintf(ftmp, "%20.10f", *((double *) pd));
          break;
      }
      did_1_col = 1;
    }
    if (did_header) {
      if (!ptbl->insertcb)
        fprintf(ftmp, " LIMIT 1 OFFSET %d\n", rx);
      else
        fprintf(ftmp, ")\n");
    }
    rx++;
    if (ptbl->iterator)
      pr = (ptbl->iterator) (pr, ptbl->it_info, rx);
    else {
      if (rx >= ptbl->nrows)
        pr = (void *) NULL;
      else
        pr = (char *)ptbl->address + (rx * sr);
    }
  }

  /* Done saving the data.  Close the file and rename it to the
     location the user requested */

  /* (BTW: we use rename() because it is guaranteed to be atomic.
     Rename() requires that both files be on the same partition; hence
     our effort to put the temp file in the same directory as the
     target file.) */
  (void) fclose(ftmp);
  if (rename(tfile, path) != 0) {
    rta_stat.nsyserr++;
    if (rta_dbg.syserr)
      rta_log(LOC, Er_No_Save, ptbl->name, path);
    return (RTA_ERROR);
  }
  return (RTA_SUCCESS);
}



/***************************************************************
 * rta_load():  - Load a table from a file of UPDATE commands.
 * 
 * Input:  ptbl - pointer to the table to be loaded
 *         fname - string with name of the load file
 *
 * Return: RTA_SUCCESS   - table loaded
 *         RTA_ERROR     - some kind of error
 **************************************************************/
int
rta_load(RTA_TBLDEF *ptbl, char *fname)
{
  extern struct RtaStat rta_stat;
  FILE    *fp;         /* FILE handle to the load file */
  char    *savefilename; /* table's savefile name */
  char     line[RTA_MX_LN_SZ]; /* input line from file */
  char     reply[RTA_MX_LN_SZ]; /* response from SQL process */
  int      nreply;     /* number of free bytes in reply */
  char     path[PATH_MAX];  /* full path/file name */

  /* We open the load file and read it one line at a time, executing
     each line that contains "UPDATE" or "INSERT" as the first word.
     (Lines not starting with UPDATE or INSERT are comments.) Note
     that any write callbacks associated with the table will be invoked.
     We hide the table's save file name, if any, in order to prevent
     the system from trying to save the table before we are done
     loading it. */


  /* Fill in the path with the full path to the config file */
  path[0] = (char) 0;
  if (fname[0] == '/') {
    (void) strncpy(path, fname, PATH_MAX);
    path[PATH_MAX -1] = (char) 0;
  }
  else {
    if (ConfigDir) {
      (void) strcpy(path, ConfigDir);
      strcat(path, "/");
    }
    strcat(path, fname);
  }

  /* Open the savefile of SQL UPDATE statements */
  fp = fopen(path, "r");
  if (fp == (FILE *) 0) {
    rta_stat.nsyserr++;
    if (rta_dbg.syserr)
      rta_log(LOC, Er_No_Load, ptbl->name, path);
    return (RTA_ERROR);
  }

  /* Don't let the DB try to save changes right now */
  savefilename = ptbl->savefile;
  ptbl->savefile = (char *) 0;

  /* process each line in the file */
  while (fgets(line, RTA_MX_LN_SZ, fp)) {
    /* A comment if first word is not UPDATE or INSERT */
    if (strncmp(line, "UPDATE ", 7) && strncmp(line, "INSERT ", 7))
      continue;

    nreply = RTA_MX_LN_SZ;
    rta_SQL_string(line, strlen(line), reply, &nreply);
    if (!strncmp(line, "UPDATE 1", 8) && !strncmp(line, "INSERT", 6)) {
      /* SQL command failed! Report error */
      rta_stat.nsyserr++;
      if (rta_dbg.syserr)
        rta_log(LOC, Er_No_Load, ptbl->name, fname);
      return (RTA_ERROR);
    }
  }

  ptbl->savefile = savefilename;

  return (RTA_SUCCESS);
}



/***************************************************************
 * is_reserved():  - Check to see if a word is one of our SQL
 * reserved words.
 * 
 * Input:  pword - pointer to the word to check
 *
 * Return: 0 if not a reserved word,  1 if it is.
 **************************************************************/
static int
is_reserved(char *pword)
{
  return (!strcasecmp(pword, "SELECT") ||
          !strcasecmp(pword, "UPDATE") ||
          !strcasecmp(pword, "DELETE") ||
          !strcasecmp(pword, "INSERT") ||
          !strcasecmp(pword, "VALUES") ||
          !strcasecmp(pword, "FROM") ||
          !strcasecmp(pword, "INTO") ||
          !strcasecmp(pword, "WHERE") ||
          !strcasecmp(pword, "LIMIT") ||
          !strcasecmp(pword, "OFFSET") ||
          !strcasecmp(pword, "SET"));
}


