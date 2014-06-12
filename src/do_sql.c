/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2004 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * do_sql.c:  The subroutines in this file execute the SQL
 * commands received from the UIs.  
 **************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>             /* for va_arg */
#include <string.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "do_sql.h"

struct Sql_Cmd cmd;

extern TBLDEF *Tbl[];
extern int Ntbl;
extern COLDEF *Col[];
extern int Ncol;
extern struct RtaStat rtastat;
extern struct RtaDbg rtadbg;

/***************************************************************
 * How this stuff works:
 *   The main program accepts TCP connections from Postgres
 * clients.  The main program collects the SQL command from
 * the clients and calls dbcommand() to handle the protocol
 * between the DB client and our embedded DB server.  The
 * routine dbcommand() handles the actual SQL by setting up
 * a lex buffer and calling yyparse() to invoke the yacc/lex
 * scanner for our micro-SQL.  The scanner parses the command
 * and places the relevant information into the "sql_cmd"
 * structure.  If the parse of the command is successful, the
 * routine do_sql() is called to actually execute the command.
 **************************************************************/

/***************************************************************
 * do_sql(): - Execute the SQL select or update command in the
 * sql_cmd structure.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      Lots.  This is where the read and write
 *               callbacks are executed.
 ***************************************************************/
void
do_sql(char *buf, int *nbuf)
{
  switch (cmd.command) {
    case RTA_SELECT:
      verify_table_name(buf, nbuf);
      if (cmd.err)
        return;
      verify_select_list(buf, nbuf);
      if (cmd.err)
        return;
      verify_where_list(buf, nbuf);
      if (cmd.err)
        return;

      /* The command looks good.  Send the Row Desc. */
      buf += send_row_description(buf, nbuf);
      if (cmd.err)
        return;
      do_select(buf, nbuf);
      rtastat.nselect++;
      break;

    case RTA_UPDATE:
      verify_table_name(buf, nbuf);
      if (cmd.err)
        return;
      verify_update_list(buf, nbuf);
      if (cmd.err)
        return;
      verify_where_list(buf, nbuf);
      if (cmd.err)
        return;

      /* The command looks good. Update and do callbacks */
      do_update(buf, nbuf);
      rtastat.nupdate++;
      break;

    default:
#ifdef HAVE_SYSLOG_H
      syslog(LOG_ERR, "DB error: no SQL cmd\n");
#endif
      break;
  }
}

/***************************************************************
 * verify_table_name(): - Verify that the table referenced in
 * the command exists.   We want to make sure everything is
 * correct before we start any of the actual command since we 
 * we don't want side effects left over from a failed command.
 * On error, we output the error message and set the err flag.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      The err flag, and the output buffer
 ***************************************************************/
void
verify_table_name(char *buf, int *nbuf)
{
  int      i;          /* temp integers */

  /* Verify that the table exists */
  for (i = 0; i < Ntbl; i++) {
    if ((Tbl[i] != (TBLDEF *) 0) &&
      (!strncmp(cmd.tbl, Tbl[i]->name, MXTBLNAME))) {
      break;
    }
  }
  if ((Tbl[i] == (TBLDEF *) 0) || (i == Ntbl)) {
    send_error(LOC, E_NOTABLE, cmd.tbl);
    return;
  }

  /* Save the pointer to the table in TBLDEFS for later use */
  cmd.itbl = i;
  cmd.ptbl = Tbl[i];
}

/***************************************************************
 * verify_select_list(): - Verify the list of column to display
 * in a select statement.  We want to make sure everything is
 * correct before we start any of the actual select since we 
 * we don't want side effects left over from a failed select.
 * This is where we compute the length of each output line so
 * we can test buffer space later.
 * On error, we output the error message and set the err flag.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      The err flag, and the output buffer
 ***************************************************************/
void
verify_select_list(char *buf, int *nbuf)
{
  COLDEF  *coldefs;    /* the table of columns */
  int      ncols;      /* the number of columns */
  int      i, j;       /* temp integers */

  /* Verify that each column exists in the right table */
  coldefs = cmd.ptbl->cols;
  ncols = cmd.ptbl->ncol;

  /* Handle the special case of a SELECT * FROM .... We look for the
     '*', then for each column in the table, free any memory in
     cmd.cols, get the size of the column name, allocate memeory, copy
     the column name, and put the pointer into cmd.cols.  */
  if (cmd.cols[0][0] == '*') {
    /* they are asking for the full column list */
    for (i = 0; i < ncols; i++) {
      if (cmd.cols[i])
        free(cmd.cols[i]);
      cmd.cols[i] = malloc(strlen(coldefs[i].name) + 1);
      if (cmd.cols[i] == (void *) 0) {
        rtastat.nsyserr++;
        if (rtadbg.syserr)
          rtalog(LOC, Er_No_Mem);
        cmd.err = 1;
        return;
      }
      strcpy(cmd.cols[i], coldefs[i].name);

      /* Save pointer to the column in COLDEFS */
      cmd.pcol[i] = &(coldefs[i]);
    }

    /* Give the command the correct # cols to display */
    cmd.ncols = ncols;
  }

  /* OK, scan the columns definitions to verify that the columns in the 
     command are really columns in the table. Scan is ...for each col
     in select list ... */
  for (j = 0; j < NCMDCOLS; j++) {
    if (cmd.cols[j] == (char *) 0) {
      return;                   /* select list is OK */
    }

    /* Scan is ...for each column definition ... */
    for (i = 0; i < ncols; i++) {
      /* Is this column in the table of interest? */
      if (!strncmp(cmd.cols[j], coldefs[i].name, MXCOLNAME)) {
        /* Save pointer to the column in COLDEFS */
        cmd.pcol[j] = &(coldefs[i]);

        /* Compute length of output line */
        switch (coldefs[i].type) {
          case RTA_STR:
          case RTA_PSTR:
            cmd.nlineout += coldefs[i].length;
            break;
          case RTA_PTR:
          case RTA_INT:
          case RTA_PINT:
            cmd.nlineout += MX_INT_STRING;
            break;
          case RTA_LONG:
          case RTA_PLONG:
            cmd.nlineout += MX_LONG_STRING;
            break;
          case RTA_FLOAT:
          case RTA_PFLOAT:
            cmd.nlineout += MX_FLOT_STRING;
            break;
        }
        cmd.nlineout += 4;      /* 4 byte length in reply */
        break;
      }
    }

    /* We scanned column defs.  Error if not found */
    if (i == ncols) {
      send_error(LOC, E_NOCOLUMN, cmd.cols[j]);
      return;
    }
  }

  /* The select column list is OK */
  return;
}

/***************************************************************
 * verify_where_list(): - Verify the relations in a WHERE
 * clause.  We want to make sure everything is correct before
 * we start any of the actual select/update since we we don't
 * want side effects left over from a failed command.
 * On error, we output the error message and set the err flag.
 * We check that each column is in the specified table, and that
 * the type of the data in each phrase (eg,  x = 12) matches the
 * data type for that column.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      The err flag and the output buffer on error
 ***************************************************************/
void
verify_where_list(char *buf, int *nbuf)
{
  COLDEF  *coldefs;    /* the table of columns */
  int      ncols;      /* the number of columns */
  int      i, j;       /* Loop index */

  /* Verify that each column exists in the right table */
  coldefs = cmd.ptbl->cols;
  ncols = cmd.ptbl->ncol;

  /* scan the columns definitions to verify that the columns in the
     command are really columns in the table. Scan is ...for each col
     in where list ... */
  for (j = 0; j < NCMDCOLS; j++) {
    if (cmd.whrcols[j] == (char *) 0) {
      return;
    }

    /* Scan is ...for each column definition ... */
    for (i = 0; i < ncols; i++) {
      /* we are on a column def for the right table */
      if (!strncmp(cmd.whrcols[j], coldefs[i].name, MXCOLNAME)) {
        /* column is valid, now check data type.  Must be string or
           num, if num, need val */
        if ((coldefs[i].type == RTA_STR) ||
          (coldefs[i].type == RTA_PSTR) || ((coldefs[i].type == RTA_INT)
            && (sscanf(cmd.whrvals[j], "%d", &(cmd.whrints[j])) == 1))
          || ((coldefs[i].type == RTA_PINT)
            && (sscanf(cmd.whrvals[j], "%d", &(cmd.whrints[j])) == 1))
          || ((coldefs[i].type == RTA_PTR)
            && (sscanf(cmd.whrvals[j], "%d", &(cmd.whrints[j])) == 1))
          || ((coldefs[i].type == RTA_LONG)
            && (sscanf(cmd.whrvals[j], "%lld", &(cmd.whrlngs[j])) == 1))
          || ((coldefs[i].type == RTA_PLONG)
            && (sscanf(cmd.whrvals[j], "%lld", &(cmd.whrlngs[j])) == 1))
          || ((coldefs[i].type == RTA_FLOAT)
            && (sscanf(cmd.whrvals[j], "%f", &(cmd.whrflot[j])) == 1))
          || ((coldefs[i].type == RTA_PFLOAT)
            && (sscanf(cmd.whrvals[j], "%f", &(cmd.whrflot[j])) == 1))) {
          /* Save WHERE column pointer for later use */
          cmd.pwhr[j] = &(coldefs[i]);
          break;
        }

        /* bogus where phrase */
        send_error(LOC, E_BADPARSE);
        return;
      }
    }

    /* We scanned column defs.  Error if not found */
    if (i == ncols) {
      send_error(LOC, E_NOCOLUMN, cmd.whrcols[j]);
      return;
    }
  }

  /* The where column list is OK */
  return;
}

/***************************************************************
 * verify_update_list(): - Verify the list of column to update
 * in an update statement.  We want to make sure everything is
 * correct before we start any of the actual select since we 
 * we don't want side effects left over from a failed select.
 * On error, we output the error message and set the err flag.
 * We verify that the columns are indeed part of the specified
 * table and that the data type of the column matches the data
 * type of the right-hand-side.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      The err flag and the output buffer on error
 ***************************************************************/
void
verify_update_list(char *buf, int *nbuf)
{
  COLDEF  *coldefs;    /* the table of columns */
  int      ncols;      /* the number of columns */
  int      i, j;       /* Loop index */

  /* Verify that each column exists in the right table */
  coldefs = cmd.ptbl->cols;
  ncols = cmd.ptbl->ncol;

  /* scan the columns definitions to verify that the columns in the
     command are really columns in the table. Scan is ...for each col
     in update list ... */
  for (j = 0; j < NCMDCOLS; j++) {
    if (cmd.cols[j] == (char *) 0) {
      return;
    }

    /* Scan is ...for each column definition ... */
    for (i = 0; i < ncols; i++) {
      if (strncmp(cmd.cols[j], coldefs[i].name, MXCOLNAME))
        continue;

      /* Save pointer to the column in COLDEFS */
      cmd.pcol[j] = &(coldefs[i]);

      /* Verify that column is not read-only */
      if (coldefs[i].flags & RTA_READONLY) {
        send_error(LOC, E_NOWRITE, coldefs[i].name);
        return;
      }

      /* Column exists and is not read-only. Now check data type.  If 
         a string, check the string length.  We do a '-1' to be sure
         there is room for a null at the end of the string.   */
      if ((coldefs[i].type == RTA_STR) || (coldefs[i].type == RTA_PSTR)) {
        if (strlen(cmd.updvals[j]) <= coldefs[i].length -1) {
          break;
        }
        send_error(LOC, E_BIGSTR, coldefs[i].name);
        return;
      }

      /* Verify conversion of int/long */
      if (((coldefs[i].type == RTA_INT)
          && (sscanf(cmd.updvals[j], "%d", &(cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_PINT)
          && (sscanf(cmd.updvals[j], "%d", &(cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_PTR)
          && (sscanf(cmd.updvals[j], "%d", &(cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_LONG)
          && (sscanf(cmd.updvals[j], "%lld", &(cmd.updlngs[j])) == 1))
        || ((coldefs[i].type == RTA_PLONG)
          && (sscanf(cmd.updvals[j], "%lld", &(cmd.updlngs[j])) == 1))
        || ((coldefs[i].type == RTA_FLOAT)
          && (sscanf(cmd.updvals[j], "%f", &(cmd.updflot[j])) == 1))
        || ((coldefs[i].type == RTA_PFLOAT)
          && (sscanf(cmd.updvals[j], "%f", &(cmd.updflot[j])) == 1))) {
        break;
      }

      /* bogus update list */
      send_error(LOC, E_BADPARSE);
      return;
    }

    /* We scanned column defs.  Error if not found */
    if (i == ncols) {
      send_error(LOC, E_NOCOLUMN, cmd.cols[j]);
      return;
    }
  }

  /* The update column list is OK */
  return;
}

/***************************************************************
 * do_select(): - Execute a SELECT statement against the DB.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      Maybe lots.  This is where the read callbacks
 *               are executed.
 ***************************************************************/
void
do_select(char *buf, int *nbuf)
{
  int      sr;         /* the Size of each Row in the table */
  int      rx;         /* Row indeX in for() loop */
  int      wx;         /* Where clause indeX in for loop */
  void    *pr;         /* Pointer to the row in the table/column */
  void    *pd;         /* Pointer to the Data in the table/column */
  llong    cmp;        /* has actual relation of col and val */
  int      dor;        /* DO Row == 1 if we should print row */
  int      npr = 0;    /* Number of output rows */
  char     nprstr[30]; /* string to hold ASCII of npr */
  char    *startbuf;   /* used to compute response length */
  char    *lenloc;     /* points to where 'D' pkt length goes */
  int      cx;         /* Column index while building Data pkt */
  int      n;          /* number of chars printed in sprintf() */

  startbuf = buf;

  /* We loop through all rows in the table in question applying the
     WHERE condition.  If a row matches we perform read callbacks on
     the selected columns and build a reply with the requested data */
  sr = cmd.ptbl->rowlen;
  rx = 0;
  if (cmd.ptbl->iterator)
    pr = (cmd.ptbl->iterator) ((void *) NULL, cmd.ptbl->it_info, rx);
  else
    pr = cmd.ptbl->address;

  /* for each row ..... */
  while (pr) {
    dor = 1;
    for (wx = 0; wx < cmd.nwhrcols; wx++) {
      /* execute read callback (if defined) on row */
      /* the call back is expected to fill in the data */
      if (cmd.pwhr[wx]->readcb) {
        (cmd.pwhr[wx]->readcb) (cmd.tbl, cmd.whrcols[wx],
          cmd.sqlcmd, pr, rx);
      }

      /* compute pointer to actual data */
      pd = (char *)pr + cmd.pwhr[wx]->offset;

      /* do comparison based on column data type */
      switch (cmd.pwhr[wx]->type) {
        case RTA_STR:
          cmp = strncmp((char *) pd, cmd.whrvals[wx],
            cmd.pwhr[wx]->length);
          break;
        case RTA_PSTR:
          cmp = strncmp(*(char **) pd, cmd.whrvals[wx],
            cmd.pwhr[wx]->length);
          break;
        case RTA_INT:
          cmp = *((int *) pd) - cmd.whrints[wx];
          break;
        case RTA_PINT:
          cmp = **((int **) pd) - cmd.whrints[wx];
          break;
        case RTA_LONG:
          cmp = *((llong *) pd) - cmd.whrlngs[wx];
          break;
        case RTA_PLONG:
          cmp = **((llong **) pd) - cmd.whrlngs[wx];
          break;
        case RTA_PTR:
          cmp = *((int *) pd) - cmd.whrints[wx];
          break;
        case RTA_FLOAT:
          cmp = *((float *) pd) - cmd.whrflot[wx];
          break;
        case RTA_PFLOAT:
          cmp = **((float **) pd) - cmd.whrflot[wx];
          break;
        default:
          cmp = 1;              /* assume no match */
          break;
      }
      if (!(((cmp == 0) && (cmd.whrrel[wx] == RTA_EQ ||
              cmd.whrrel[wx] == RTA_GE ||
              cmd.whrrel[wx] == RTA_LE)) ||
          ((cmp != 0) && (cmd.whrrel[wx] == RTA_NE)) ||
          ((cmp < 0) && (cmd.whrrel[wx] == RTA_LE ||
              cmd.whrrel[wx] == RTA_LT)) ||
          ((cmp > 0) && (cmd.whrrel[wx] == RTA_GE ||
              cmd.whrrel[wx] == RTA_GT)))) {
        dor = 0;
        break;
      }
    }
    if (dor && cmd.offset)
      cmd.offset--;
    else if (dor) {
      /* if we get here, we've passed the WHERE clause and OFFSET
         filtering.  Check the LIMIT filter */
      if (npr >= cmd.limit) {
        break;
      }

      /* Verify that the buffer has enough room for this row.  Note
         that we've been adding the maximum field length for each
         column into nlineout so that it now contains a worst case
         line length for this table/row.  */
      if (*nbuf - ((int) (buf - startbuf)) - cmd.nlineout < 100) {
      /* 100 for CSELECT and margin */
        send_error(LOC, E_FULLBUF);
        return;
      }

      /* At this point we have a row which passed the * WHERE clause,
         is greater than OFFSET and less * than LIMIT. So send it! */
      *buf++ = 'D';             /* Data packet */
      lenloc = buf;             /* Remember location for length */
      buf += 4;                 /* Response length goes here */
      ad_int2(&buf, cmd.ncols); /* # of cols in response */

      for (cx = 0; cx < cmd.ncols; cx++) {
        /* execute column read callback (if defined). callback will
           fill in the data if needed */
        if (cmd.pcol[cx]->readcb) {
          (cmd.pcol[cx]->readcb) (cmd.tbl,
            cmd.pcol[cx]->name, cmd.sqlcmd, pr, rx);
        }

        /* compute pointer to actual data */
        pd = (char *)pr + cmd.pcol[cx]->offset;
        switch ((cmd.pcol[cx])->type) {
          case RTA_STR:
            /* send 4 byte length.  Include the length */
            ad_int4(&buf, strlen(pd));
            ad_str(&buf, pd);   /* send the response */
            break;
          case RTA_PSTR:
            ad_int4(&buf, strlen(*(char **) pd));
            /* send the response */
            ad_str(&buf, *(char **) pd);
            break;
          case RTA_INT:
            n = sprintf((buf + 4), "%d", *((int *) pd));
            ad_int4(&buf, n);   /* send length */
            buf += n;
            break;
          case RTA_PINT:
            n = sprintf((buf + 4), "%d", **((int **) pd));
            ad_int4(&buf, n);   /* send length */
            buf += n;
            break;
          case RTA_LONG:
            n = sprintf((buf + 4), "%lld", *((llong *) pd));
            ad_int4(&buf, n);
            buf += n;
            break;
          case RTA_PLONG:
#ifndef __BORLANDC__
            n = sprintf((buf + 4), "%lld", *((llong *) pd));
#else
            n = sprintf((buf + 4), "%ld", *((llong *) pd));
#endif
            ad_int4(&buf, n);
            buf += n;
            break;
          case RTA_PTR:
            n = sprintf((buf + 4), "%d", *((int *) pd));
            ad_int4(&buf, n);   /* send length */
            buf += n;
            break;
          case RTA_FLOAT:
            n = sprintf((buf + 4), "%20.10f", *((float *) pd));
            ad_int4(&buf, n);
            buf += n;
            break;
          case RTA_PFLOAT:
            n = sprintf((buf + 4), "%20.10f", **((float **) pd));
            ad_int4(&buf, n);
            buf += n;
            break;
        }
      }
      /* now fill in 'D' response length */
      ad_int4(&lenloc, (int) (buf - lenloc));
      npr++;
    }
    rx++;
    if (cmd.ptbl->iterator)
      pr = (cmd.ptbl->iterator) (pr, cmd.ptbl->it_info, rx);
    else {
      if (rx >= cmd.ptbl->nrows)
        pr = (void *) NULL;
      else
        pr = (char *)cmd.ptbl->address + (rx * sr);
    }
  }
  /* Add 'C', length(11), 'SELECT', NULL to output */
  *buf++ = 'C';
  ad_int4(&buf, 11);            /* 11= 4+strlen(SELECT)+1 */
  ad_str(&buf, "SELECT");
  *buf++ = 0x00;

  *nbuf -= (int) (buf - startbuf);

  /* Log SQL if trace is on */
  if (rtadbg.trace) {
    (void) sprintf(nprstr, "%d", npr);
    rtalog(LOC, Er_Trace_SQL, cmd.sqlcmd, nprstr);
  }
}

/***************************************************************
 * send_row_description(): - We have analyzed the select command
 * and it seems OK.  We start the reply by sending the row
 * description first.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       Returns the number of bytes written to buffer
 * Effects:      Lots.  This is where the write callbacks 
 *               are executed.
 ***************************************************************/
int
send_row_description(char *buf, int *nbuf)
{
  char    *startbuf;   /* used to compute response length */
  int      i;          /* loop index */
  int      size;       /* estimated/actual size of response */

  startbuf = buf;

  /* Verify that the buffer has enough room for this reply. (See the
     Postgres protocol description for an understanding of the next
     two lines.) */
  size = 7;                     /* sizeof 'T', length, and int2 */
  size += cmd.ncols * (MXCOLNAME + 1 + 4 + 2 + 4 + 2 + 4 + 2);

  if (*nbuf - size < 100) {     /* 100 just for safety */
    send_error(LOC, E_FULLBUF);
    return ((int) (cmd.out - startbuf)); /* ignored */
  }

  /* Send the row description header */
  *buf++ = 'T';                 /* row description */
  buf += 4;                     /* put pkt length here later */
  ad_int2(&buf, cmd.ncols);     /* num fields */

  for (i = 0; i < cmd.ncols; i++) {
    ad_str(&buf, cmd.cols[i]);  /* column name */
    *buf++ = (char) 0;          /* send the NULL */

    /* Add table index */
    ad_int4(&buf, cmd.itbl);

    /* Add the column index */
    ad_int2(&buf, i);

    /* OIDs are tbl index times max col + col index */
    ad_int4(&buf, (cmd.itbl * NCMDCOLS) + i);

    /* set size/modifier based on type */
    switch ((cmd.pcol[i])->type) {
      case RTA_STR:
      case RTA_PSTR:
        ad_int2(&buf, -1);      /* length */
        ad_int4(&buf, 29);      /* type modifier */
        break;
      case RTA_INT:
      case RTA_PINT:
        ad_int2(&buf, sizeof(int)); /* length */
        ad_int4(&buf, -1);      /* type modifier */
        break;
      case RTA_LONG:
      case RTA_PLONG:
        ad_int2(&buf, sizeof(llong)); /* length */
        ad_int4(&buf, -1);      /* type modifier */
        break;
      case RTA_FLOAT:
      case RTA_PFLOAT:
        ad_int2(&buf, sizeof(float)); /* length */
        ad_int4(&buf, -1);      /* type modifier */
        break;
      case RTA_PTR:
        ad_int2(&buf, sizeof(void *)); /* length */
        ad_int4(&buf, -1);      /* type modifier */
        break;
    }

    /* Add the format type.  0==text format */
    ad_int2(&buf, 0);
  }
  size = (int) (buf - startbuf); /* actual response size */
  *nbuf -= size;

  /* store packet length -1 (the 'T' is not included *) */
  startbuf++;                   /* skip over the 'T' */
  ad_int4(&startbuf, (size - 1));

  return (size);
}

/***************************************************************
 * send_error(): - Send an error message back to the requesting
 * program or process.  
 *
 * Input:        The file name and line number where the error
 *               was detected, and the format and optional
 *               argument of the error message.
 * Output:       void
 * Effects:      Puts data in the command response buffer.
 ***************************************************************/
void
send_error(char *filename, int lineno, char *fmt, char *arg)
{
  int      cnt;        /* a byte count of printed chars */
  int      len;        /* length of error message */
  char    *lenptr;     /* where to put the length */

  cmd.err = 1;
  rtastat.nsqlerr++;
  if (rtadbg.sqlerr)
    rtalog(filename, lineno, Er_Bad_SQL, cmd.sqlcmd);

  /* Make sure we have enough space for the output.  Current #defines
     limit the maximum message to less than 100 bytes */
  if (*cmd.nout < 100) {
    return;                     /* not much else we can do...  */
  }

  /* We want to overwrite any output so far.  We do this by */
  /* pointing the output buffer back to its original value. */
  cmd.out = cmd.errout;         /* Reset any output so far */
  *(cmd.nout) = cmd.nerrout;

  /* The format of the error message is 'E', int32 for length, a series 
     of parameters including 'S'everity, error 'C'ode, and 'M'essage.
     Parameters are delimited with nulls and there is an extra null at 
     the end to indicate that there are no more parameters.  . */

  *cmd.out++ = 'E';
  lenptr = cmd.out;             /* msg length goes here */
  cmd.out += 4;                 /* skip over length for now */
  ad_str(&(cmd.out), "SERROR"); /* severity code */
  *cmd.out++ = (char) 0;
  ad_str(&(cmd.out), "C42601"); /* error code (syntax error) */
  *cmd.out++ = (char) 0;
  *cmd.out++ = 'M';
  cnt = snprintf(cmd.out, *(cmd.nout), fmt, arg);
  cmd.out += cnt;
  cmd.out++;                    /* to include the NULL */
  *cmd.out++ = (char) 0;        /* terminate param list */
  len = (int) (cmd.out - cmd.errout) - 1; /* -1 to exclude E */
  ad_int4(&lenptr, len);
  *cmd.nout -= (int) (cmd.out - cmd.errout);

  return;
}

/***************************************************************
 * do_update(): - Execute the SQL update command in the
 * sql_cmd structure.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      Lots.  This is where the write callbacks 
 *               are executed.
 ***************************************************************/
void
do_update(char *buf, int *nbuf)
{
  int      sr;         /* the Size of each Row in the table */
  int      rx;         /* Row indeX in for() loop */
  int      wx;         /* Where clause indeX in for loop */
  void    *pr;         /* Pointer to the row in the table/column */
  void    *pd;         /* Pointer to the Data in the table/column */
  void    *poldrow;    /* Pointer to copy of row before update */
  llong    cmp;        /* has actual relation of col and val */
  int      dor;        /* DO Row == 1 if we should update row */
  char    *startbuf;   /* used to compute response length */
  int      cx;         /* Column index while building Data pkt */
  int      n;          /* number of chars printed in sprintf() */
  int      nru = 0;    /* =# rows updated */
  int      svt = 0;    /* Save table if == 1 */
  char    *tmark;      /* Address of U in "CUPDATE" if success */

  startbuf = buf;

  /* We loop through all rows in the table in question applying the
     WHERE condition.  If a row matches we update the appropriate
     columns and call any write callbacks */
  sr = cmd.ptbl->rowlen;
  rx = 0;
  if (cmd.ptbl->iterator)
    pr = (cmd.ptbl->iterator) ((void *) NULL, cmd.ptbl->it_info, rx);
  else
    pr = cmd.ptbl->address;

  /* for each row ..... */
  while (pr) {
    dor = 1;
    for (wx = 0; wx < cmd.nwhrcols; wx++) {
      /* The WHERE clause ...... execute read callback (if defined) on
         row * the call back is expected to fill in the data */
      if (cmd.pwhr[wx]->readcb) {
        (cmd.pwhr[wx]->readcb) (cmd.tbl, cmd.whrcols[wx],
          cmd.sqlcmd, pr, rx);
      }

      /* compute pointer to actual data */
      pd = (char *)pr + cmd.pwhr[wx]->offset;

      /* do comparison based on column data type */
      switch (cmd.pwhr[wx]->type) {
        case RTA_STR:
          cmp = strncmp((char *) pd, cmd.whrvals[wx],
            cmd.pwhr[wx]->length);
          break;
        case RTA_PSTR:
          cmp = strcmp(*(char **) pd, cmd.whrvals[wx]);
          break;
        case RTA_INT:
          cmp = *((int *) pd) - cmd.whrints[wx];
          break;
        case RTA_PINT:
          cmp = **((int **) pd) - cmd.whrints[wx];
          break;
        case RTA_LONG:
          cmp = *((llong*) pd) - cmd.whrlngs[wx];
          break;
        case RTA_PLONG:
          cmp = **((long**) pd) - cmd.whrlngs[wx];
          break;
        case RTA_FLOAT:
          cmp = *((float *) pd) - cmd.whrflot[wx];
          break;
        case RTA_PFLOAT:
          cmp = **((float **) pd) - cmd.whrflot[wx];
          break;
        case RTA_PTR:
          cmp = *((int *) pd) - cmd.whrints[wx];
          break;
        default:
          cmp = 1;              /* assume no match */
          break;
      }
      if (!(((cmp == 0) && (cmd.whrrel[wx] == RTA_EQ ||
              cmd.whrrel[wx] == RTA_GE ||
              cmd.whrrel[wx] == RTA_LE)) ||
          ((cmp != 0) && (cmd.whrrel[wx] == RTA_NE)) ||
          ((cmp < 0) && (cmd.whrrel[wx] == RTA_LE ||
              cmd.whrrel[wx] == RTA_LT)) ||
          ((cmp > 0) && (cmd.whrrel[wx] == RTA_GE ||
              cmd.whrrel[wx] == RTA_GT)))) {
        dor = 0;
        break;
      }
    }
    if (dor && cmd.offset)
      cmd.offset--;
    else if (dor) {             /* DO Row */
      /* if we get here, we've passed the WHERE clause and the OFFSET.
         Check for LIMIT filtering */
      if (cmd.limit <= 0) {
        break;
      }

      /* At this point we have a row which passed the * WHERE clause,
         is greater than OFFSET and less * than LIMIT. So update it! */

      /* Save the data from the old row for use in the write callback */
      poldrow = malloc(cmd.ptbl->rowlen);
      if (poldrow)
        memcpy(poldrow, pr, cmd.ptbl->rowlen);
      else {
        rtalog(LOC, Er_No_Mem);
        return;
      }

      /* Scan the columns doing updates as needed */
      for (cx = 0; cx < cmd.ncols; cx++) {
        /* compute pointer to actual data */
        pd = (char *)pr + cmd.pcol[cx]->offset;

        switch ((cmd.pcol[cx])->type) {
          case RTA_STR:
            strncpy((char *) pd, cmd.updvals[cx], cmd.pcol[cx]->length);
            break;
          case RTA_PSTR:
            strncpy(*(char **) pd, cmd.updvals[cx],
              cmd.pcol[cx]->length);
            break;
          case RTA_INT:
            *((int *) pd) = cmd.updints[cx];
            break;
          case RTA_PINT:
            **((int **) pd) = cmd.updints[cx];
            break;
          case RTA_LONG:
            *((llong *) pd) = cmd.updlngs[cx];
            break;
          case RTA_PLONG:
            **((llong **) pd) = cmd.updlngs[cx];
            break;
          case RTA_PTR:
            /* works only if INT and PTR are same size */
            *((int *) pd) = cmd.updints[cx];
            break;
          case RTA_FLOAT:
            *((float *) pd) = cmd.updflot[cx];
            break;
          case RTA_PFLOAT:
            **((float **) pd) = cmd.updflot[cx];
            break;
        }
        if (cmd.pcol[cx]->flags & RTA_DISKSAVE)
          svt = 1;
      }

      /* We call the write callbacks after all of the * columns have
         been updated.  */
      for (cx = 0; cx < cmd.ncols; cx++) {
        /* execute write callback (if defined) on row. callback will
           fill in the data if needed */
        if (cmd.pcol[cx]->writecb) {
          (cmd.pcol[cx]->writecb) (cmd.tbl, cmd.pcol[cx]->name,
           cmd.sqlcmd, pr, rx, poldrow);
        }
      }
      if (poldrow)       /* free the image of the last row */
        free(poldrow);
      cmd.limit--;       /* decrement row limit count */
      nru++;
    }
    rx++;
    if (cmd.ptbl->iterator)
      pr = (cmd.ptbl->iterator) (pr, cmd.ptbl->it_info, rx);
    else {
      if (rx >= cmd.ptbl->nrows)
        pr = (void *) NULL;
      else
        pr = (char *)cmd.ptbl->address + (rx * sr);
    }
  }

  /* Save the table to disk if needed */
  if (svt && cmd.ptbl->savefile && strlen(cmd.ptbl->savefile))
    rta_save(cmd.ptbl, cmd.ptbl->savefile);

  /* Send the update complete message */
  *buf++ = 'C';
  tmark = buf;                  /* Save length location */
  buf += 4;
  ad_str(&buf, "UPDATE");
  n = sprintf(buf, " %d", nru); /* # rows affected */
  buf += n;
  *buf++ = 0x00;
  ad_int4(&tmark, (buf - tmark));

  *nbuf -= (int) (buf - startbuf);

  /* Log SQL if trace is on.  (+7 skips over 'UPDATE ') */
  if (rtadbg.trace)
    rtalog(LOC, Er_Trace_SQL, cmd.sqlcmd, (tmark + 7));
}

/***************************************************************
 * ad_str(): - Add a string to the output buffer.  Includes a
 *             NULL to terminate the string.
 *
 * Input:        A **char to the buffer and the string
 * Output:       void
 * Effects:      Increments the **char to point to the next
 *               available space in the buffer.
 ***************************************************************/
void
ad_str(char **pbuf, char *instr)
{
  strcpy(*pbuf, instr);
  *pbuf += strlen(instr);
  **pbuf = (char) 0;
}

/***************************************************************
 * ad_int2(): - Add a 2 byte integer to the output buffer
 *
 * Input:        A **char to the buffer and the integer
 * Output:       void
 * Effects:      Increments the **char to point to the next
 *               available space in the buffer.
 ***************************************************************/
void
ad_int2(char **pbuf, int inint)
{
  **pbuf = (char) ((inint >> 8) & 0x00FF);
  (*pbuf)++;
  **pbuf = (char) (inint & 0x00FF);
  (*pbuf)++;
}

/***************************************************************
 * ad_int4(): - Add a 4 byte integer to the output buffer
 *
 * Input:        A **char to the buffer and the integer
 * Output:       void
 * Effects:      Increments the **char to point to the next
 *               available space in the buffer.
 ***************************************************************/
void
ad_int4(char **pbuf, int inint)
{
  **pbuf = (char) ((inint >> 24) & 0x00FF);
  (*pbuf)++;
  **pbuf = (char) ((inint >> 16) & 0x00FF);
  (*pbuf)++;
  **pbuf = (char) ((inint >> 8) & 0x00FF);
  (*pbuf)++;
  **pbuf = (char) (inint & 0x00FF);
  (*pbuf)++;
}

/***************************************************************
 * rtalog(): - Sends debug log messages to syslog() and stderr.
 *
 * Output:       void
 * Effects:      Increments the **char to point to the next
 *               available space in the buffer.
 ***************************************************************/
void
rtalog(char *fname,    /* error detected in file... */
  int linen,           /* error detected at line # */
  char *format, ...)
{                               /* printf format string */
  extern struct RtaDbg rtadbg;
  va_list  ap;
  char    *s1;         /* first optional argument */
  char    *s2;         /* second optional argument */
  char    *sptr;       /* used to look for %s */

  s1 = (char *) 0;
  s2 = (char *) 0;

  /* Get the optional parameters if any */
  va_start(ap, format);
  sptr = strstr(format, "%s");
  if (sptr) {
    s1 = va_arg(ap, char *);

    sptr++;
    if (sptr)
      s2 = va_arg(ap, char *);
  }
  va_end(ap);

#ifdef HAVE_SYSLOG_H
  /* Send to syslog() if so configured */
  if (rtadbg.target == 1 || rtadbg.target == 3)
    syslog(rtadbg.priority, format, fname, linen, s1, s2);
#endif

  /* Send to stderr if so configured */
  if (rtadbg.target == 2 || rtadbg.target == 3) {
    fprintf(stderr, format, fname, linen, s1, s2);
    fprintf(stderr, "\n");
  }
}
