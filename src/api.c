
/***************************************************************
 * Run Time Access
 * Copyright (C) 2003 Robert W Smith (bsmith@linuxtoys.org)
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
#include "rta.h"                /* for various constants */
#include "do_sql.h"             /* for LOC */

/* Tbl and Col contain pointers to table and column
 * definitions of all tables and columns in * the system.
 * Ntbl and Ncol are the number of tables and columns in each
 * list.  These are used often enough that they are globals. */
TBLDEF  *Tbl[MX_TBL];
int      Ntbl;
COLDEF  *Col[MX_COL];
int      Ncol;

extern struct EpgDbg rtadbg;

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
  extern TBLDEF pg_userTable;
  extern TBLDEF rta_tablesTable;
  extern TBLDEF rta_columnsTable;
  extern TBLDEF rta_dbgTable;
  extern TBLDEF rta_statTable;
  extern void restart_syslog();

  for (i = 0; i < MX_TBL; i++)
  {
    Tbl[i] = (TBLDEF *) 0;
  }
  Ntbl = 0;

  /* add system and internal tables here */
  (void) rta_add_table(&rta_tablesTable);
  (void) rta_add_table(&rta_columnsTable);
  (void) rta_add_table(&pg_userTable);
  (void) rta_add_table(&rta_dbgTable);
  (void) rta_add_table(&rta_statTable);

  restart_syslog((char *) 0, (char *) 0, (char *) 0, 0);
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
rta_add_table(TBLDEF *ptbl)
{
  extern struct EpgStat rtastat;
  extern TBLDEF rta_columnsTable;
  int      i, j;       /* a loop index */

  /* Error if at Ntbl limit */
  if (Ntbl == MX_TBL)
  {
    rtastat.nrtaerr++;
    if (rtadbg.rtaerr)
      rtalog(LOC, Er_Max_Tbls);
    return (RTA_ERROR);
  }

  /* verify that table name is unique */
  i = 0;
  while (i < Ntbl)
  {
    if (!strncmp(ptbl->name, Tbl[i]->name, MXTBLNAME))
    {
      rtastat.nrtaerr++;
      if (rtadbg.rtaerr)
        rtalog(LOC, Er_Tbl_Dup, ptbl->name);
      return (RTA_ERROR);
    }
    i++;
  }

  /* verify lenght of table name */
  if (strlen(ptbl->name) > MXTBLNAME)
  {
    rtastat.nrtaerr++;
    if (rtadbg.rtaerr)
      rtalog(LOC, Er_Tname_Big, ptbl->name);
    return (RTA_ERROR);
  }

  /* verify savefile name is a valid pointer */
  if (ptbl->savefile == (char *) 0)
  {
    rtastat.nrtaerr++;
    if (rtadbg.rtaerr)
      rtalog(LOC, Er_Col_Type, "savefile");
    return (RTA_ERROR);
  }

  /* Check the upper bound on # columns / table */
  if (ptbl->ncol > NCMDCOLS)
  {
    rtastat.nrtaerr++;
    if (rtadbg.rtaerr)
      rtalog(LOC, Er_Cmd_Cols, ptbl->name);
    return (RTA_ERROR);
  }

  /* verify that column names are unique within table */
  for (i = 0; i < ptbl->ncol; i++)
  {
    for (j = 0; j < i; j++)
    {
      if (!strncmp(ptbl->cols[i].name, ptbl->cols[j].name, MXCOLNAME))
      {
        rtastat.nrtaerr++;
        if (rtadbg.rtaerr)
          rtalog(LOC, Er_Col_Dup, ptbl->cols[i].name);
        return (RTA_ERROR);
      }
    }
  }

  /* verify column name length, help length, data type, and flag
     contents */
  for (i = 0; i < ptbl->ncol; i++)
  {
    if (strlen(ptbl->cols[i].name) > MXCOLNAME)
    {
      rtastat.nrtaerr++;
      if (rtadbg.rtaerr)
        rtalog(LOC, Er_Cname_Big, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
    if (strlen(ptbl->cols[i].help) > MXHELPSTR)
    {
      rtastat.nrtaerr++;
      if (rtadbg.rtaerr)
        rtalog(LOC, Er_Hname_Big, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
    if (ptbl->cols[i].type > MXCOLTYPE)
    {
      rtastat.nrtaerr++;
      if (rtadbg.rtaerr)
        rtalog(LOC, Er_Col_Type, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
    if (ptbl->cols[i].flags > RTA_DISKSAVE + RTA_READONLY)
    {
      rtastat.nrtaerr++;
      if (rtadbg.rtaerr)
        rtalog(LOC, Er_Col_Flag, ptbl->cols[i].name);
      return (RTA_ERROR);
    }
  }

  /* Verify that we can add the columns */
  if ((Ncol + ptbl->ncol) >= MX_COL)
  {
    rtastat.nrtaerr++;
    if (rtadbg.rtaerr)
      rtalog(LOC, Er_Max_Cols);
    return (RTA_ERROR);
  }

  /* Everything looks OK.  Add table and columns */
  Tbl[Ntbl++] = ptbl;
  Tbl[0]->nrows = Ntbl;

  /* Add columns to list of column pointers */
  for (i = 0; i < ptbl->ncol; i++)
  {
    Col[Ncol++] = &(ptbl->cols[i]);
  }
  rta_columnsTable.nrows += ptbl->ncol;

  /* Execute commands in the save file to restore */
  if (ptbl->savefile && strlen(ptbl->savefile) > 0)
    (void) rta_load(ptbl, ptbl->savefile);

  return (RTA_SUCCESS);
}

/***************************************************************
 * Postgres "packets" are identified by their first few bytes.
 * The newer protocol used a single ASCII byte to identify the
 * packet type, while the older protocol has a 32 bit length
 * field at the start of the packet.  Note that multi-byte data
 * is sent with the most significant byte first.  Please see the
 * full documentation in "PostgreSQL 7.2.1 Developer's Guide"
 * at http://www.postgresql.org/idocs/
 * 
 * The Postgres protocol from client to server has about six
 * request types.  We use three of the request types in our
 * basic implementation.  The six packet types.....
 * BYTE0  BYTE1  BYTE2  BYTE3
 *     0      0    0x1   0x18  Startup packet to open connection
 *     0      ?      ?      ?  Encrypted password packet
 *     0      0      0   0x10  Cancel pending request
 *   'F'                       Function call
 *   'Q'                       Query
 *   'X'                       Terminate connection
 *
 **************************************************************/

/***************************************************************
 * dbcommand():  - Depacketize and execute any Postgres 
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
 **************************************************************/
int
dbcommand(char *buf, int *nin, char *out, int *nout)
{
  extern struct EpgStat rtastat;
  int      length;     /* lenght of the packet if old protocol */
  int      i;          /* a temp integer */

  /* old style packet if first byte is zero */
  if ((int) buf[0] == 0)
  {
    /* get length.  Enough bytes for a length? if not, consume no
       input, write no output */
    if (*nin < 4)
    {
      return (RTA_NOCMD);
    }
    length = (int) (buf[3] + (buf[2] << 8) + (buf[1] << 16));

    /* Is the whole packet here? If not, consume no input, write no
       output */
    if (*nin < length)
    {
      return (RTA_NOCMD);
    }
    if (length == 296)          /* a startup request */
    {
      /* we key on a non-null user name to send AuthOK. The protocol
         packet has an int32 for the length, an int32 for the protocol
         version, a 64 char string for the DB name and at byte 72 the
         start of a 32 char user name. */
      if (buf[72] == (char) 0)
      {
        *nin -= length;
        out[0] = 'N';           /* "Notice" response */
        *nout -= 1;
        rtastat.nauth++;
      }
      else
      {
        *nin -= length;
        out[0] = 'R';
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
        out[4] = 0;
        out[5] = 'Z';
        // *out++ = 'R';
        // ad_int4 (&buf, 0);
        // *out++ = 'Z';
        *nout -= 6;
      }
      return (RTA_SUCCESS);
    }
    else if (length == 16)      /* a cancel request */
    {
      /* ignore the request for now */
      *nin -= length;
      return (RTA_SUCCESS);
    }
    else                        /* should be a password */
    {
      *nin -= length;
      return (RTA_SUCCESS);
    }
  }
  else if (buf[0] == 'Q')       /* a query request */
  {
    /* check for a complete command */
    for (i = 0; i < *nin; i++)
    {
      if (buf[i] == (char) 0)
        break;
    }
    if (i == *nin)
    {
      return (RTA_NOCMD);
    }

    /* Got a null terminated command; do it. (buf[1] since the SQL
       follows the 'Q') */
    SQL_string(&buf[1], out, nout);
    *nin -= strlen(buf);        /* to swallow the cmd */
    (*nin)--;                   /* to swallow the null */
    return (RTA_SUCCESS);
  }
  else if (buf[0] == 'X')       /* a terminate request */
  {
    return (RTA_CLOSE);
  }
  else if (buf[0] == 'F')       /* a function request */
  {
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
rta_save(TBLDEF *ptbl, char *fname)
{
  extern struct EpgStat rtastat;
  int      sr;         /* the Size of each Row in the table */
  int      rx;         /* Row indeX in for() loop */
  void    *pd;         /* Pointer to the Data in the table/column */
  int      cx;         /* Column index while building Data pkt */
  char     tfile[PATH_MAX];
  char     path[PATH_MAX];
  int      fd;         /* file descriptor of temp file */
  FILE    *ftmp;       /* FILE handle to the temp file */
  int      did_header; /* == 1 if printed UPDATE part */
  int      did_1_col;  /* == 1 if at least one col printed */

  /* Open a temp file in the same directory as the users target file */
  (void) strncpy(path, fname, PATH_MAX);
  (void) strncpy(tfile, dirname(path), PATH_MAX);
  (void) strcat(tfile, "/tmpXXXXXX");
  fd = mkstemp(tfile);
  if (fd < 0)
  {
    rtastat.nsyserr++;
    if (rtadbg.syserr)
      rtalog(LOC, Er_No_Save, tfile);
    return (RTA_ERROR);
  }
  ftmp = fdopen(fd, "w");
  if (ftmp == (FILE *) 0)
  {
    rtastat.nsyserr++;
    if (rtadbg.syserr)
      rtalog(LOC, Er_No_Save, tfile);
    return (RTA_ERROR);
  }

  /* OK, temp file is open and ready to receive table data */
  sr = ptbl->rowlen;

  for (rx = 0; rx < ptbl->nrows; rx++)
  {
    did_header = 0;
    did_1_col = 0;
    for (cx = 0; cx < ptbl->ncol; cx++)
    {
      if (!ptbl->cols[cx].flags & RTA_DISKSAVE)
        continue;
      if (!did_header)
      {
        fprintf(ftmp, "UPDATE %s SET", ptbl->name);
        did_header = 1;
      }
      if (!did_1_col)
        fprintf(ftmp, " %s ", ptbl->cols[cx].name);
      else
        fprintf(ftmp, ", %s ", ptbl->cols[cx].name);

      /* compute pointer to actual data */
      pd = ptbl->address + (rx * sr) + ptbl->cols[cx].offset;
      switch ((ptbl->cols[cx]).type)
      {
        case RTA_STR:
          if (memchr((char *) pd, '"', ptbl->cols[cx].length))
            fprintf(ftmp, "= \'%s\'", (char *) pd);
          else
            fprintf(ftmp, "= \"%s\"", (char *) pd);
          break;
        case RTA_PSTR:
          if (memchr((char *) pd, '"', ptbl->cols[cx].length))
            fprintf(ftmp, "= \'%s\'", *(char **) pd);
          else
            fprintf(ftmp, "= \"%s\"", *(char **) pd);
          break;
        case RTA_INT:
          fprintf(ftmp, "= %d", *((int *) pd));
          break;
        case RTA_PINT:
          fprintf(ftmp, "= %d", **((int **) pd));
          break;
        case RTA_LONG:
          fprintf(ftmp, "= %lld", *((long long *) pd));
          break;
        case RTA_PLONG:
          fprintf(ftmp, "= %lld", **((long long **) pd));
          break;
        case RTA_PTR:

          /* works only if INT and PTR are same size */
          fprintf(ftmp, "= %d", *((int *) pd));
          break;
        case RTA_FLOAT:
          fprintf(ftmp, "= %20.10f", *((float *) pd));
          break;
        case RTA_PFLOAT:
          fprintf(ftmp, "= %20.10f", **((float **) pd));
          break;
      }
      did_1_col = 1;
    }
    if (did_header)
      fprintf(ftmp, " LIMIT 1 OFFSET %d\n", rx);
  }

  /* Done saving the data.  Close the file and rename it to the
     location the user requested */

  /* (BTW: we use rename() because it is guaranteed to be atomic.
     Rename() requires that both files be on the same partition; hence
     our effort to put the temp file in the same directory as the
     target file.) */
  (void) fclose(ftmp);
  if (rename(tfile, fname) != 0)
  {
    rtastat.nsyserr++;
    if (rtadbg.syserr)
      rtalog(LOC, Er_No_Save, fname);
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
rta_load(TBLDEF *ptbl, char *fname)
{
  extern struct EpgStat rtastat;
  FILE    *fp;         /* FILE handle to the load file */
  char    *savefilename; /* table's savefile name */
  char     line[MX_LN_SZ]; /* input line from file */
  char     reply[MX_LN_SZ]; /* response from SQL process */
  int      nreply;     /* number of free bytes in reply */

  /* We open the load file and read it one line at a time, executing
     each line that contains "UPDATE" as the first word.  (Lines not
     starting with UPDATE are comments.) Note that any write callbacks
     associated with the table will be invoked. We hide the table's
     save file name, if any, in order to prevent the system from trying 
     to save the table before we are done loading it. */
  fp = fopen(fname, "r");
  if (fp == (FILE *) 0)
  {
    rtastat.nsyserr++;
    if (rtadbg.syserr)
      rtalog(LOC, Er_No_Load, fname);
    return (RTA_ERROR);
  }

  /* Don't let the DB try to save changes right now */
  savefilename = ptbl->savefile;
  ptbl->savefile = (char *) 0;

  /* process each line in the file */
  while (fgets(line, MX_LN_SZ, fp))
  {
    /* A comment if first word is not "UPDATE " */
    if (strncmp(line, "UPDATE ", 7))
      continue;

    nreply = MX_LN_SZ;
    SQL_string(line, reply, &nreply);
    if (!strncmp(line, "UPDATE 1", 8))
    {
      /* SQL command failed! Report error */
      rtastat.nsyserr++;
      if (rtadbg.syserr)
        rtalog(LOC, Er_No_Load, fname);
      return (RTA_ERROR);
    }
  }

  ptbl->savefile = savefilename;

  return (RTA_SUCCESS);
}
