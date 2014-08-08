/***************************************************************
 * librta library
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * do_sql.c:  The subroutines in this file execute the SQL
 * commands received from the UIs.  
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>             /* for va_arg */
#include <string.h>
#include <syslog.h>
#include "do_sql.h"

struct Sql_Cmd rta_cmd;

extern RTA_TBLDEF *rta_Tbl[];
extern int rta_Ntbl;
extern RTA_COLDEF *rta_Col[];
extern int rta_Ncol;

/* Forward references */
static void     verify_table_name(char *, int *);
static void     verify_select_list(char *, int *);
static void     verify_update_list(char *, int *);
static void     verify_insert_list(char *, int *);
static void     verify_where_list(char *, int *);
static void     verify_insert_callback(char *, int *);
static void     verify_delete_callback(char *, int *);
static void     do_update(char *, int *);
static void     do_insert(char *, int *);
static void     free_row(RTA_TBLDEF *, void *);
static void     do_delete(char *, int *);
static void     do_select(char *, int *);
static int      send_row_description(char *, int *);
static void     do_delete(char *, int *);
static void     ad_str(char **, char *, int);
static void     ad_int2(char **, int);
static void     ad_int4(char **, int);


/***************************************************************
 * How this stuff works:
 *   The main program accepts TCP connections from Postgres
 * clients.  The main program collects the SQL command from
 * the clients and calls rta_dbcommand() to handle the protocol
 * between the DB client and our embedded DB server.  The
 * routine rta_dbcommand() handles the actual SQL by setting up
 * a lex buffer and calling yyparse() to invoke the yacc/lex
 * scanner for our micro-SQL.  The scanner parses the command
 * and places the relevant information into the "sql_cmd"
 * structure.  If the parse of the command is successful, the
 * routine do_sql() is called to actually execute the command.
 **************************************************************/

/***************************************************************
 * rta_do_sql(): - Execute the SQL select or update command in the
 * sql_cmd structure.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      Lots.  This is where the read and write
 *               callbacks are executed.
 ***************************************************************/
void
rta_do_sql(char *buf, int *nbuf)
{
  switch (rta_cmd.command) {
    case RTA_SELECT:
      verify_table_name(buf, nbuf);
      if (rta_cmd.err)
        return;
      verify_select_list(buf, nbuf);
      if (rta_cmd.err)
        return;
      verify_where_list(buf, nbuf);
      if (rta_cmd.err)
        return;

      /* The command looks good.  Send the Row Desc. */
      buf += send_row_description(buf, nbuf);
      if (rta_cmd.err)
        return;
      do_select(buf, nbuf);
      break;

    case RTA_UPDATE:
      verify_table_name(buf, nbuf);
      if (rta_cmd.err)
        return;
      verify_update_list(buf, nbuf);
      if (rta_cmd.err)
        return;
      verify_where_list(buf, nbuf);
      if (rta_cmd.err)
        return;

      /* The command looks good. Update and do callbacks */
      do_update(buf, nbuf);
      break;

    case RTA_INSERT:
      verify_table_name(buf, nbuf);
      if (rta_cmd.err)
        return;
      verify_insert_callback(buf, nbuf);
      if (rta_cmd.err)
        return;
      verify_insert_list(buf, nbuf);
      if (rta_cmd.err)
        return;
      /* The command looks good. INSERT the row. */
      do_insert(buf, nbuf);
      break;

    case RTA_DELETE:
      verify_table_name(buf, nbuf);
      if (rta_cmd.err)
        return;
      verify_where_list(buf, nbuf);
      if (rta_cmd.err)
        return;
      verify_delete_callback(buf, nbuf);
      if (rta_cmd.err)
        return;
      /* The command looks good.  Delete the rows */
      do_delete(buf, nbuf);
      break;

    default:
      syslog(LOG_ERR, "DB error: no SQL cmd\n");
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
static void
verify_table_name(char *buf, int *nbuf)
{
  int      i;          /* temp integers */

  /* Verify that the table exists */
  for (i = 0; i < rta_Ntbl; i++) {
    if ((rta_Tbl[i] != (RTA_TBLDEF *) 0) &&
      (!strncmp(rta_cmd.tbl, rta_Tbl[i]->name, RTA_MXTBLNAME))) {
      break;
    }
  }
  if ((rta_Tbl[i] == (RTA_TBLDEF *) 0) || (i == rta_Ntbl)) {
    rta_send_error(LOC, E_NOTABLE, rta_cmd.tbl);
    return;
  }

  /* Save the pointer to the table in RTA_TBLDEFS for later use */
  rta_cmd.itbl = i;
  rta_cmd.ptbl = rta_Tbl[i];
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
static void
verify_select_list(char *buf, int *nbuf)
{
  RTA_COLDEF  *coldefs;    /* the table of columns */
  int          ncols;      /* the number of columns */
  int          i, j;       /* temp integers */

  /* Verify that each column exists in the right table */
  coldefs = rta_cmd.ptbl->cols;
  ncols = rta_cmd.ptbl->ncol;

  /* Handle the special case of a SELECT * FROM .... We look for the
     '*', then for each column in the table, free any memory in
     rta_cmd.cols, get the size of the column name, allocate memeory, copy
     the column name, and put the pointer into rta_cmd.cols.  */
  if (rta_cmd.cols[0][0] == '*') {
    /* they are asking for the full column list */
    for (i = 0; i < ncols; i++) {
      if (rta_cmd.cols[i])
        free(rta_cmd.cols[i]);
      rta_cmd.cols[i] = malloc(strlen(coldefs[i].name) + 1);
      if (rta_cmd.cols[i] == (void *) 0) {
        rta_log(LOC, Er_No_Mem);
        rta_cmd.err = 1;
        return;
      }
      strcpy(rta_cmd.cols[i], coldefs[i].name);

      /* Save pointer to the column in RTA_COLDEFS */
      rta_cmd.pcol[i] = &(coldefs[i]);
    }

    /* Give the command the correct # cols to display */
    rta_cmd.ncols = ncols;
  }

  /* OK, scan the columns definitions to verify that the columns in the 
     command are really columns in the table. Scan is ...for each col
     in select list ... */
  for (j = 0; j < RTA_NCMDCOLS; j++) {
    if (rta_cmd.cols[j] == (char *) 0) {
      return;                   /* select list is OK */
    }

    /* Scan is ...for each column definition ... */
    for (i = 0; i < ncols; i++) {
      /* Is this column in the table of interest? */
      if (!strncmp(rta_cmd.cols[j], coldefs[i].name, RTA_MXCOLNAME)) {
        /* Save pointer to the column in RTA_COLDEFS */
        rta_cmd.pcol[j] = &(coldefs[i]);

        /* Compute length of output line */
        switch (coldefs[i].type) {
          case RTA_STR:
          case RTA_PSTR:
            rta_cmd.nlineout += coldefs[i].length;
            break;
          case RTA_PTR:
          case RTA_INT:
          case RTA_PINT:
	  case RTA_SHORT:
	  case RTA_UCHAR:
            rta_cmd.nlineout += MX_INT_STRING;
            break;
          case RTA_LONG:
          case RTA_PLONG:
            rta_cmd.nlineout += MX_LONG_STRING;
            break;
          case RTA_FLOAT:
          case RTA_PFLOAT:
	  case RTA_DOUBLE:
            rta_cmd.nlineout += MX_FLOT_STRING;
            break;
        }
        rta_cmd.nlineout += 4;      /* 4 byte length in reply */
        break;
      }
    }

    /* We scanned column defs.  Error if not found */
    if (i == ncols) {
      rta_send_error(LOC, E_NOCOLUMN, rta_cmd.cols[j]);
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
static void
verify_where_list(char *buf, int *nbuf)
{
  RTA_COLDEF  *coldefs;    /* the table of columns */
  int          ncols;      /* the number of columns */
  int          i, j;       /* Loop index */

  /* Verify that each column exists in the right table */
  coldefs = rta_cmd.ptbl->cols;
  ncols = rta_cmd.ptbl->ncol;

  /* scan the columns definitions to verify that the columns in the
     command are really columns in the table. Scan is ...for each col
     in where list ... */
  for (j = 0; j < RTA_NCMDCOLS; j++) {
    if (rta_cmd.whrcols[j] == (char *) 0) {
      return;
    }

    /* Scan is ...for each column definition ... */
    for (i = 0; i < ncols; i++) {
      /* we are on a column def for the right table */
      if (!strncmp(rta_cmd.whrcols[j], coldefs[i].name, RTA_MXCOLNAME)) {
        /* column is valid, now check data type.  Must be string or
           num, if num, need val */
        if ((coldefs[i].type == RTA_STR) ||
          (coldefs[i].type == RTA_PSTR) ||
	    (((coldefs[i].type == RTA_INT) || (coldefs[i].type == RTA_SHORT)
	      || (coldefs[i].type == RTA_UCHAR)) &&
	     (sscanf(rta_cmd.whrvals[j], "%d", &(rta_cmd.whrints[j])) == 1))
          || ((coldefs[i].type == RTA_PINT)
            && (sscanf(rta_cmd.whrvals[j], "%d", &(rta_cmd.whrints[j])) == 1))
          || ((coldefs[i].type == RTA_PTR)
            && (sscanf(rta_cmd.whrvals[j], "%d", &(rta_cmd.whrints[j])) == 1))
          || ((coldefs[i].type == RTA_LONG)
            && (sscanf(rta_cmd.whrvals[j], "%lld", &(rta_cmd.whrlngs[j])) == 1))
          || ((coldefs[i].type == RTA_PLONG)
            && (sscanf(rta_cmd.whrvals[j], "%lld", &(rta_cmd.whrlngs[j])) == 1))
          || ((coldefs[i].type == RTA_FLOAT)
            && (sscanf(rta_cmd.whrvals[j], "%f", &(rta_cmd.whrflot[j])) == 1))
          || ((coldefs[i].type == RTA_PFLOAT)
            && (sscanf(rta_cmd.whrvals[j], "%f", &(rta_cmd.whrflot[j])) == 1))
          || ((coldefs[i].type == RTA_DOUBLE)
            && (sscanf(rta_cmd.whrvals[j], "%lf", &(rta_cmd.whrdbl[j])) == 1))) {
          /* Save WHERE column pointer for later use */
          rta_cmd.pwhr[j] = &(coldefs[i]);
          break;
        }

        /* bogus where phrase */
        rta_send_error(LOC, E_BADPARSE);
        return;
      }
    }

    /* We scanned column defs.  Error if not found */
    if (i == ncols) {
      rta_send_error(LOC, E_NOCOLUMN, rta_cmd.whrcols[j]);
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
static void
verify_update_list(char *buf, int *nbuf)
{
  RTA_COLDEF  *coldefs;    /* the table of columns */
  int          ncols;      /* the number of columns */
  int          i, j;       /* Loop index */

  /* Verify that each column exists in the right table */
  coldefs = rta_cmd.ptbl->cols;
  ncols = rta_cmd.ptbl->ncol;

  /* scan the columns definitions to verify that the columns in the
     command are really columns in the table. Scan is ...for each col
     in update list ... */
  for (j = 0; j < RTA_NCMDCOLS; j++) {
    if (rta_cmd.cols[j] == (char *) 0) {
      return;
    }

    /* Scan is ...for each column definition ... */
    for (i = 0; i < ncols; i++) {
      if (strncmp(rta_cmd.cols[j], coldefs[i].name, RTA_MXCOLNAME))
        continue;

      /* Save pointer to the column in RTA_COLDEFS */
      rta_cmd.pcol[j] = &(coldefs[i]);

      /* Verify that column is not read-only */
      if (coldefs[i].flags & RTA_READONLY) {
        rta_send_error(LOC, E_NOWRITE, coldefs[i].name);
        return;
      }

      /* Column exists and is not read-only. Now check data type.  If 
         a string, check the string length.  We do a '-1' to be sure
         there is room for a null at the end of the string.   */
      if ((coldefs[i].type == RTA_STR) || (coldefs[i].type == RTA_PSTR)) {
        if (strlen(rta_cmd.updvals[j]) <= coldefs[i].length -1) {
          break;
        }
        rta_send_error(LOC, E_BIGSTR, coldefs[i].name);
        return;
      }

      /* Verify conversion of int/long */
      if ((((coldefs[i].type == RTA_INT) || (coldefs[i].type == RTA_SHORT)
	    || (coldefs[i].type == RTA_UCHAR))
          && (sscanf(rta_cmd.updvals[j], "%d", &(rta_cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_PINT)
          && (sscanf(rta_cmd.updvals[j], "%d", &(rta_cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_PTR)
          && (sscanf(rta_cmd.updvals[j], "%d", &(rta_cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_LONG)
          && (sscanf(rta_cmd.updvals[j], "%lld", &(rta_cmd.updlngs[j])) == 1))
        || ((coldefs[i].type == RTA_PLONG)
          && (sscanf(rta_cmd.updvals[j], "%lld", &(rta_cmd.updlngs[j])) == 1))
        || ((coldefs[i].type == RTA_FLOAT)
          && (sscanf(rta_cmd.updvals[j], "%f", &(rta_cmd.updflot[j])) == 1))
        || ((coldefs[i].type == RTA_PFLOAT)
          && (sscanf(rta_cmd.updvals[j], "%f", &(rta_cmd.updflot[j])) == 1))
        || ((coldefs[i].type == RTA_DOUBLE)
          && (sscanf(rta_cmd.updvals[j], "%lf", &(rta_cmd.upddbl[j])) == 1))) {
        break;
      }

      /* bogus update list */
      rta_send_error(LOC, E_BADPARSE);
      return;
    }

    /* We scanned column defs.  Error if not found */
    if (i == ncols) {
      rta_send_error(LOC, E_NOCOLUMN, rta_cmd.cols[j]);
      return;
    }
  }

  /* The update column list is OK */
  return;
}


/***************************************************************
 * verify_insert_list(): - Verify the list of column to insert
 * in an insert statement.  We want to make sure everything is
 * correct before we start any of the actual insert since we 
 * we don't want side effects left over from a failed insert.
 * On error, we output the error message and set the err flag.
 * We verify that the columns are indeed part of the specified
 * table, that the number of columns matches the number of values,
 * and that the data type of the column matches the data type of
 * corresponding values field.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      The err flag and the output buffer on error
 ***************************************************************/
static void
verify_insert_list(char *buf, int *nbuf)
{
  RTA_COLDEF  *coldefs;    /* the table of columns */
  int          ncols;      /* the number of columns */
  int          i, j;       /* Loop index */

  /* Verify that each column exists in the right table */
  coldefs = rta_cmd.ptbl->cols;
  ncols = rta_cmd.ptbl->ncol;

  /* scan the columns definitions to verify that the columns in the
     command are really columns in the table. Scan is ...for each col
     in update list ... */
  for (j = 0; j < RTA_NCMDCOLS; j++) {
    if (rta_cmd.cols[j] == (char *) 0) {
      return;
    }

    /* Scan is ...for each column definition ... */
    for (i = 0; i < ncols; i++) {
      if (strncmp(rta_cmd.cols[j], coldefs[i].name, RTA_MXCOLNAME))
        continue;

      /* Save pointer to the column in RTA_COLDEFS */
      rta_cmd.pcol[j] = &(coldefs[i]);

      /* Column exists. Now check data type.  If 
         a string, check the string length.  We do a '-1' to be sure
         there is room for a null at the end of the string.   */
      if ((coldefs[i].type == RTA_STR) || (coldefs[i].type == RTA_PSTR)) {
        if (strlen(rta_cmd.updvals[j]) <= coldefs[i].length -1) {
          break;
        }
        rta_send_error(LOC, E_BIGSTR, coldefs[i].name);
        return;
      }

      /* Verify that column is not read-only */
      if (coldefs[i].flags & RTA_READONLY) {
        rta_send_error(LOC, E_NOWRITE, coldefs[i].name);
        return;
      }

      /* Verify conversion of int/long */
      if ((((coldefs[i].type == RTA_INT) || (coldefs[i].type == RTA_SHORT)
	    || (coldefs[i].type == RTA_UCHAR))
          && (sscanf(rta_cmd.updvals[j], "%d", &(rta_cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_PINT)
          && (sscanf(rta_cmd.updvals[j], "%d", &(rta_cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_PTR)
          && (sscanf(rta_cmd.updvals[j], "%d", &(rta_cmd.updints[j])) == 1))
        || ((coldefs[i].type == RTA_LONG)
          && (sscanf(rta_cmd.updvals[j], "%lld", &(rta_cmd.updlngs[j])) == 1))
        || ((coldefs[i].type == RTA_PLONG)
          && (sscanf(rta_cmd.updvals[j], "%lld", &(rta_cmd.updlngs[j])) == 1))
        || ((coldefs[i].type == RTA_FLOAT)
          && (sscanf(rta_cmd.updvals[j], "%f", &(rta_cmd.updflot[j])) == 1))
        || ((coldefs[i].type == RTA_PFLOAT)
          && (sscanf(rta_cmd.updvals[j], "%f", &(rta_cmd.updflot[j])) == 1))
        || ((coldefs[i].type == RTA_DOUBLE)
          && (sscanf(rta_cmd.updvals[j], "%lf", &(rta_cmd.upddbl[j])) == 1))) {
        break;
      }

      /* bogus update list */
      rta_send_error(LOC, E_BADPARSE);
      return;
    }

    /* We scanned column defs.  Error if not found */
    if (i == ncols) {
      rta_send_error(LOC, E_NOCOLUMN, rta_cmd.cols[j]);
      return;
    }
  }

  /* The update column list is OK */
  return;
}

/***************************************************************
 * verify_insert_callback(): - Verify that the table referenced
 * in the command has an insert callback.   We want to make sure
 * everything is correct before we start any of the actual command
 * since we we don't want side effects left over from a failed
 * command.
 * On error, we output the error message and set the err flag.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      The err flag, and the output buffer
 ***************************************************************/
static void
verify_insert_callback(char *buf, int *nbuf)
{
  /* Verify that the table has an insert callback */
  if (! rta_cmd.ptbl->insertcb) {
    rta_send_error(LOC, E_NOINSERT, rta_cmd.tbl);
    return;
  }
}


/***************************************************************
 * verify_delete_callback(): - Verify that the table referenced
 * in the command has a delete callback.   We want to make sure
 * everything is correct before we start any of the actual command
 * since we we don't want side effects left over from a failed
 * command.
 * On error, we output the error message and set the err flag.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      The err flag, and the output buffer
 ***************************************************************/
static void
verify_delete_callback(char *buf, int *nbuf)
{
  /* Verify that the table has a delete callback */
  if (! rta_cmd.ptbl->deletecb) {
    rta_send_error(LOC, E_NODELETE, rta_cmd.tbl);
    return;
  }
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
static void
do_select(char *buf, int *nbuf)
{
  int      sr;         /* the Size of each Row in the table */
  int      rx;         /* Row indeX in for() loop */
  int      wx;         /* Where clause indeX in for loop */
  void    *pr;         /* Pointer to the row in the table/column */
  void    *pd;         /* Pointer to the Data in the table/column */
  llong cmp;           /* has actual relation of col and val */
  int      dor;        /* DO Row == 1 if we should print row */
  int      npr = 0;    /* Number of output rows */
  char    *startbuf;   /* used to compute response length */
  char    *lenloc;     /* points to where 'D' pkt length goes */
  int      cx;         /* Column index while building Data pkt */
  int      n;          /* number of chars printed in sprintf() */
  int      count;      /* number of chars to send as string */

  startbuf = buf;

  /* We loop through all rows in the table in question applying the
     WHERE condition.  If a row matches we perform read callbacks on
     the selected columns and build a reply with the requested data */
  sr = rta_cmd.ptbl->rowlen;
  rx = 0;
  if (rta_cmd.ptbl->iterator)
    pr = (rta_cmd.ptbl->iterator) ((void *) NULL, rta_cmd.ptbl->it_info, rx);
  else
    pr = rta_cmd.ptbl->address;

  /* for each row ..... */
  while (pr) {
    dor = 1;
    for (wx = 0; wx < rta_cmd.nwhrcols; wx++) {
      /* execute read callback (if defined) on row */
      /* the call back is expected to fill in the data */
      /* and return zero on success. */
      if (rta_cmd.pwhr[wx]->readcb) {
        if ((rta_cmd.pwhr[wx]->readcb) (rta_cmd.tbl, rta_cmd.whrcols[wx],
            rta_cmd.sqlcmd, pr, rx) != 0) {
          rta_send_error(LOC, E_BADTRIG, rta_cmd.whrcols[wx]);
          return;
        }
      }

      /* compute pointer to actual data */
      pd = (char *)pr + rta_cmd.pwhr[wx]->offset;

      /* do comparison based on column data type */
      switch (rta_cmd.pwhr[wx]->type) {
        case RTA_STR:
          cmp = strncmp((char *) pd, rta_cmd.whrvals[wx],
			rta_cmd.pwhr[wx]->length);
          break;
        case RTA_PSTR:
          cmp = strncmp(*(char **) pd, rta_cmd.whrvals[wx],
			rta_cmd.pwhr[wx]->length);
          break;
        case RTA_INT:
          cmp = *((int *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_SHORT:
          cmp = *((short *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_UCHAR:
          cmp = *((unsigned char *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_PINT:
          cmp = **((int **) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_LONG:
          cmp = *((llong *) pd) - rta_cmd.whrlngs[wx];
          break;
        case RTA_PLONG:
          cmp = **((llong **) pd) - rta_cmd.whrlngs[wx];
          break;
        case RTA_PTR:
          cmp = *((int *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_FLOAT:
          cmp = *((float *) pd) - rta_cmd.whrflot[wx];
          break;
        case RTA_PFLOAT:
          cmp = **((float **) pd) - rta_cmd.whrflot[wx];
          break;
        case RTA_DOUBLE:
          cmp = *((double *) pd) - rta_cmd.whrdbl[wx];
          break;
        default:
          cmp = 1;              /* assume no match */
          break;
      }
      if (!(((cmp == 0) && (rta_cmd.whrrel[wx] == RTA_EQ ||
              rta_cmd.whrrel[wx] == RTA_GE ||
              rta_cmd.whrrel[wx] == RTA_LE)) ||
          ((cmp != 0) && (rta_cmd.whrrel[wx] == RTA_NE)) ||
          ((cmp < 0) && (rta_cmd.whrrel[wx] == RTA_LE ||
              rta_cmd.whrrel[wx] == RTA_LT)) ||
          ((cmp > 0) && (rta_cmd.whrrel[wx] == RTA_GE ||
              rta_cmd.whrrel[wx] == RTA_GT)))) {
        dor = 0;
        break;
      }
    }
    if (dor && rta_cmd.offset)
      rta_cmd.offset--;
    else if (dor) {
      /* if we get here, we've passed the WHERE clause and OFFSET
         filtering.  Check the LIMIT filter */
      if (npr >= rta_cmd.limit) {
        break;
      }

      /* Verify that the buffer has enough room for this row.  Note
         that we've been adding the maximum field length for each
         column into nlineout so that it now contains a worst case
         line length for this table/row.  */
      if (*nbuf - ((int) (buf - startbuf)) - rta_cmd.nlineout < 100) {
      /* 100 for CSELECT and margin */
        rta_send_error(LOC, E_FULLBUF);
        return;
      }

      /* At this point we have a row which passed the * WHERE clause,
         is greater than OFFSET and less * than LIMIT. So send it! */
      *buf++ = 'D';             /* Data packet */
      lenloc = buf;             /* Remember location for length */
      buf += 4;                 /* Response length goes here */
      ad_int2(&buf, rta_cmd.ncols); /* # of cols in response */

      for (cx = 0; cx < rta_cmd.ncols; cx++) {
        /* execute column read callback (if defined). callback will
           fill in the data if needed, and return 0 on success */
        if (rta_cmd.pcol[cx]->readcb) {
          if ((rta_cmd.pcol[cx]->readcb) (rta_cmd.tbl,
            rta_cmd.pcol[cx]->name, rta_cmd.sqlcmd, pr, rx) != 0) {
            rta_send_error(LOC, E_BADTRIG, rta_cmd.pcol[cx]->name);
            return;
          }
        }

        /* compute pointer to actual data */
        pd = (char *)pr + rta_cmd.pcol[cx]->offset;
        switch ((rta_cmd.pcol[cx])->type) {
          case RTA_STR:
            /* send 4 byte length.  Include the length */
            count = strlen(pd);  /* shorter of field length or strlen */
            if (count > rta_cmd.pcol[cx]->length -1) {
              count = rta_cmd.pcol[cx]->length -1;
            }
            ad_int4(&buf, count);
            ad_str(&buf, pd, count);   /* send the response */
            break;
          case RTA_PSTR:
            count = strlen(*(char **) pd);  /* shorter of field length or strlen */
            if (count > rta_cmd.pcol[cx]->length -1) {
              count = rta_cmd.pcol[cx]->length -1;
            }
            ad_int4(&buf, count);
            /* send the response */
            ad_str(&buf, *(char **) pd, count);
            break;
          case RTA_INT:
            n = sprintf((buf + 4), "%d", *((int *) pd));
            ad_int4(&buf, n);   /* send length */
            buf += n;
            break;
          case RTA_SHORT:
            n = sprintf((buf + 4), "%d", *((short *) pd));
            ad_int4(&buf, n);   /* send length */
            buf += n;
            break;
          case RTA_UCHAR:
            n = sprintf((buf + 4), "%d", *((unsigned char *) pd));
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
            n = sprintf((buf + 4), "%lld", **((llong **) pd));
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
          case RTA_DOUBLE:
            n = sprintf((buf + 4), "%20.10f", *((double *) pd));
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
    if (rta_cmd.ptbl->iterator)
      pr = (rta_cmd.ptbl->iterator) (pr, rta_cmd.ptbl->it_info, rx);
    else {
      if (rx >= rta_cmd.ptbl->nrows)
        pr = (void *) NULL;
      else
        pr = (char *) rta_cmd.ptbl->address + (rx * sr);
    }
  }
  /* Add 'C', length(11), 'SELECT', NULL to output */
  *buf++ = 'C';
  ad_int4(&buf, 11);            /* 11= 4+strlen(SELECT)+1 */
  ad_str(&buf, "SELECT", 6);
  *buf++ = 0x00;

  *nbuf -= (int) (buf - startbuf);
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
static int
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
  size += rta_cmd.ncols * (RTA_MXCOLNAME + 1 + 4 + 2 + 4 + 2 + 4 + 2);

  if (*nbuf - size < 100) {     /* 100 just for safety */
    rta_send_error(LOC, E_FULLBUF);
    return ((int) (rta_cmd.out - startbuf)); /* ignored */
  }

  /* Send the row description header */
  *buf++ = 'T';                 /* row description */
  buf += 4;                     /* put pkt length here later */
  ad_int2(&buf, rta_cmd.ncols);     /* num fields */

  for (i = 0; i < rta_cmd.ncols; i++) {
    ad_str(&buf, rta_cmd.cols[i], strlen(rta_cmd.cols[i]));  /* column name */
    *buf++ = (char) 0;          /* send the NULL */

    /* Add table index */
    ad_int4(&buf, rta_cmd.itbl);

    /* Add the column index */
    ad_int2(&buf, i);

    /* OIDs are tbl index times max col + col index */
    ad_int4(&buf, (rta_cmd.itbl * RTA_NCMDCOLS) + i);

    /* set size/modifier based on type */
    switch ((rta_cmd.pcol[i])->type) {
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
      case RTA_SHORT:
        ad_int2(&buf, sizeof(short)); /* length */
        ad_int4(&buf, -1);      /* type modifier */
        break;
      case RTA_UCHAR:
        ad_int2(&buf, sizeof(unsigned char)); /* length */
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
      case RTA_DOUBLE:
        ad_int2(&buf, sizeof(double)); /* length */
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
 * rta_send_error(): - Send an error message back to the requesting
 * program or process.  
 *
 * Input:        The file name and line number where the error
 *               was detected, and the format and optional
 *               argument of the error message.
 * Output:       void
 * Effects:      Puts data in the command response buffer.
 ***************************************************************/
void
rta_send_error(char *filename, int lineno, char *fmt, char *arg)
{
  int      cnt;        /* a byte count of printed chars */
  int      len;        /* length of error message */
  char    *lenptr;     /* where to put the length */

  rta_cmd.err = 1;
  rta_log(filename, lineno, Er_Bad_SQL, rta_cmd.sqlcmd);

  /* Make sure we have enough space for the output.  Current #defines
     limit the maximum message to less than 100 bytes */
  if (*rta_cmd.nout < 100) {
    return;                     /* not much else we can do...  */
  }

  /* We want to overwrite any output so far.  We do this by */
  /* pointing the output buffer back to its original value. */
  rta_cmd.out = rta_cmd.errout;         /* Reset any output so far */
  *(rta_cmd.nout) = rta_cmd.nerrout;

  /* The format of the error message is 'E', int32 for length, a series 
     of parameters including 'S'everity, error 'C'ode, and 'M'essage.
     Parameters are delimited with nulls and there is an extra null at 
     the end to indicate that there are no more parameters.  . */

  *rta_cmd.out++ = 'E';
  lenptr = rta_cmd.out;             /* msg length goes here */
  rta_cmd.out += 4;                 /* skip over length for now */
  ad_str(&(rta_cmd.out), "SERROR", 6); /* severity code */
  *rta_cmd.out++ = (char) 0;
  ad_str(&(rta_cmd.out), "C42601", 6); /* error code (syntax error) */
  *rta_cmd.out++ = (char) 0;
  *rta_cmd.out++ = 'M';
  cnt = snprintf(rta_cmd.out, *(rta_cmd.nout), fmt, arg);
  rta_cmd.out += cnt;
  rta_cmd.out++;                    /* to include the NULL */
  *rta_cmd.out++ = (char) 0;        /* terminate param list */
  len = (int) (rta_cmd.out - rta_cmd.errout) - 1; /* -1 to exclude E */
  ad_int4(&lenptr, len);
  *rta_cmd.nout -= (int) (rta_cmd.out - rta_cmd.errout);

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
static void
do_update(char *buf, int *nbuf)
{
  int      sr;         /* the Size of each Row in the table */
  int      rx;         /* Row indeX in for() loop */
  int      wx;         /* Where clause indeX in for loop */
  void    *pr;         /* Pointer to the row in the table/column */
  void    *pd;         /* Pointer to the Data in the table/column */
  void    *poldrow;    /* Pointer to copy of row before update */
  llong cmp;           /* has actual relation of col and val */
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
  sr = rta_cmd.ptbl->rowlen;
  rx = 0;
  if (rta_cmd.ptbl->iterator)
    pr = (rta_cmd.ptbl->iterator) ((void *) NULL, rta_cmd.ptbl->it_info, rx);
  else
    pr = rta_cmd.ptbl->address;

  /* for each row ..... */
  while (pr) {
    dor = 1;
    for (wx = 0; wx < rta_cmd.nwhrcols; wx++) {
      /* The WHERE clause ...... execute read callback (if defined) on
         row * the call back is expected to fill in the data */
      if (rta_cmd.pwhr[wx]->readcb) {
        if ((rta_cmd.pwhr[wx]->readcb) (rta_cmd.tbl, rta_cmd.whrcols[wx],
            rta_cmd.sqlcmd, pr, rx) != 0) {
          rta_send_error(LOC, E_BADTRIG, rta_cmd.whrcols[wx]);
          return;
        }
      }

      /* compute pointer to actual data */
      pd = (char *)pr + rta_cmd.pwhr[wx]->offset;

      /* do comparison based on column data type */
      switch (rta_cmd.pwhr[wx]->type) {
        case RTA_STR:
          cmp = strncmp((char *) pd, rta_cmd.whrvals[wx],
			rta_cmd.pwhr[wx]->length);
          break;
        case RTA_PSTR:
          cmp = strcmp(*(char **) pd, rta_cmd.whrvals[wx]);
          break;
        case RTA_INT:
          cmp = *((int *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_SHORT:
          cmp = *((short *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_UCHAR:
          cmp = *((unsigned char *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_PINT:
          cmp = **((int **) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_LONG:
          cmp = *((llong *) pd) - rta_cmd.whrlngs[wx];
          break;
        case RTA_PLONG:
          cmp = **((llong **) pd) - rta_cmd.whrlngs[wx];
          break;
        case RTA_FLOAT:
          cmp = *((float *) pd) - rta_cmd.whrflot[wx];
          break;
        case RTA_PFLOAT:
          cmp = **((float **) pd) - rta_cmd.whrflot[wx];
          break;
        case RTA_PTR:
          cmp = *((int *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_DOUBLE:
          cmp = *((double *) pd) - rta_cmd.whrdbl[wx];
          break;
        default:
          cmp = 1;              /* assume no match */
          break;
      }
      if (!(((cmp == 0) && (rta_cmd.whrrel[wx] == RTA_EQ ||
              rta_cmd.whrrel[wx] == RTA_GE ||
              rta_cmd.whrrel[wx] == RTA_LE)) ||
          ((cmp != 0) && (rta_cmd.whrrel[wx] == RTA_NE)) ||
          ((cmp < 0) && (rta_cmd.whrrel[wx] == RTA_LE ||
              rta_cmd.whrrel[wx] == RTA_LT)) ||
          ((cmp > 0) && (rta_cmd.whrrel[wx] == RTA_GE ||
              rta_cmd.whrrel[wx] == RTA_GT)))) {
        dor = 0;
        break;
      }
    }
    if (dor && rta_cmd.offset)
      rta_cmd.offset--;
    else if (dor) {             /* DO Row */
      /* if we get here, we've passed the WHERE clause and the OFFSET.
         Check for LIMIT filtering */
      if (rta_cmd.limit <= 0) {
        break;
      }

      /* At this point we have a row which passed the * WHERE clause,
         is greater than OFFSET and less * than LIMIT. So update it! */

      /* Save the data from the old row for use in the write callback */
      poldrow = malloc(rta_cmd.ptbl->rowlen);
      if (poldrow)
        memcpy(poldrow, pr, rta_cmd.ptbl->rowlen);
      else {
        rta_log(LOC, Er_No_Mem);
        return;
      }

      /* Scan the columns doing updates as needed */
      for (cx = 0; cx < rta_cmd.ncols; cx++) {
        /* compute pointer to actual data */
        pd = (char *)pr + rta_cmd.pcol[cx]->offset;

        switch ((rta_cmd.pcol[cx])->type) {
          case RTA_STR:
            strncpy((char *) pd, rta_cmd.updvals[cx],
		    rta_cmd.pcol[cx]->length);
            break;
          case RTA_PSTR:
            strncpy(*(char **) pd, rta_cmd.updvals[cx],
		    rta_cmd.pcol[cx]->length);
            break;
          case RTA_INT:
            *((int *) pd) = rta_cmd.updints[cx];
            break;
          case RTA_SHORT:
            *((short *) pd) = rta_cmd.updints[cx];
            break;
          case RTA_UCHAR:
            *((unsigned char *) pd) = rta_cmd.updints[cx];
            break;
          case RTA_PINT:
            **((int **) pd) = rta_cmd.updints[cx];
            break;
          case RTA_LONG:
            *((llong *) pd) = rta_cmd.updlngs[cx];
            break;
          case RTA_PLONG:
            **((llong **) pd) = rta_cmd.updlngs[cx];
            break;
          case RTA_PTR:
            /* works only if INT and PTR are same size */
            *((int *) pd) = rta_cmd.updints[cx];
            break;
          case RTA_FLOAT:
            *((float *) pd) = rta_cmd.updflot[cx];
            break;
          case RTA_PFLOAT:
            **((float **) pd) = rta_cmd.updflot[cx];
            break;
          case RTA_DOUBLE:
            *((double *) pd) = rta_cmd.upddbl[cx];
            break;
        }
        if (rta_cmd.pcol[cx]->flags & RTA_DISKSAVE)
          svt = 1;
      }

      /* We call the write callbacks after all of the columns have
         been updated.  */
      for (cx = 0; cx < rta_cmd.ncols; cx++) {
        /* execute write callback (if defined) on row. callback will
           perform post processing on row and return zero on success */
        if (rta_cmd.pcol[cx]->writecb) {
          if ((rta_cmd.pcol[cx]->writecb) (rta_cmd.tbl, rta_cmd.pcol[cx]->name,
              rta_cmd.sqlcmd, pr, rx, poldrow) != 0) {
            /* restore row from saved image of it */
            memcpy(pr, poldrow, rta_cmd.ptbl->rowlen);
            free(poldrow);
            rta_send_error(LOC, E_BADTRIG, rta_cmd.pcol[cx]->name);
            return;
          }
        }
      }
      if (poldrow)       /* free the image of the last row */
        free(poldrow);
      rta_cmd.limit--;       /* decrement row limit count */
      nru++;
    }
    rx++;
    if (rta_cmd.ptbl->iterator)
      pr = (rta_cmd.ptbl->iterator) (pr, rta_cmd.ptbl->it_info, rx);
    else {
      if (rx >= rta_cmd.ptbl->nrows)
        pr = (void *) NULL;
      else
        pr = (char *)rta_cmd.ptbl->address + (rx * sr);
    }
  }

  /* Save the table to disk if needed */
  if (svt && rta_cmd.ptbl->savefile && strlen(rta_cmd.ptbl->savefile))
    rta_save(rta_cmd.ptbl, rta_cmd.ptbl->savefile);

  /* Send the update complete message */
  *buf++ = 'C';
  tmark = buf;                  /* Save length location */
  buf += 4;
  ad_str(&buf, "UPDATE", 6);
  n = sprintf(buf, " %d", nru); /* # rows affected */
  buf += n;
  *buf++ = 0x00;
  ad_int4(&tmark, (buf - tmark));

  *nbuf -= (int) (buf - startbuf);
}


/***************************************************************
 * do_insert(): - Execute the SQL insert command in the
 * sql_cmd structure.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      Lots.  This is where the write callbacks 
 *               are executed.
 ***************************************************************/
static void
do_insert(char *buf, int *nbuf)
{
  void    *pr;         /* Pointer to the row in the table/column */
  void    *pd;         /* Pointer to the Data in the table/column */
  char    *startbuf;   /* used to compute response length */
  int      cx;         /* Column index while building Data pkt */
  int      n;          /* number of chars printed in sprintf() */
  int      svt = 0;    /* Save table if == 1 */
  char    *tmark;      /* Address of U in "CINSERT" if success */
  int      rx;         /* row index returned from the insert cb */

  startbuf = buf;

  /* The basic approach is to ...                             */
  /* - Allocate memory for the row and fill it in             */
  /* - Call the insert callback to tie row to table           */
  /* - If the callback succeeds, call all the write callbacks */

  /* Allocate row and then allocate space for each pointer type */
  pr = malloc(rta_cmd.ptbl->rowlen);
  if (pr == (void *) 0) {
    rta_log(LOC, Er_No_Mem);
    rta_cmd.err = 1;
    return;
  }
  (void) memset(pr, 0, rta_cmd.ptbl->rowlen);

  /* Got the row, now walk the column definitions allocating
     space for pointer types, and initializing everything. */
  for (cx = 0; cx < rta_cmd.ptbl->ncol; cx++) {
    /* compute pointer to actual data */
    pd = pr + rta_cmd.ptbl->cols[cx].offset;

    /* Switch on data type alloc/init field */
    switch ((rta_cmd.ptbl->cols[cx]).type) {
      case RTA_STR:
        /* string was set to zero in row init memset() call */
        break;
      case RTA_PSTR:
        *(void **)pd = malloc(rta_cmd.ptbl->cols[cx].length);
        if (*(void **)pd == (void **) 0) {
          rta_log(LOC, Er_No_Mem);
          rta_cmd.err = 1;
          free_row(rta_cmd.ptbl, pr);
          return;
        }
        (void) memset(*(char **)pd, 0, rta_cmd.ptbl->cols[cx].length);
        break;
      case RTA_INT:
        *(int *)pd = (int) 0;
        break;
      case RTA_PINT:
        *(void **) pd = malloc(sizeof(int));
        if (*(void **)pd == (void **) 0) {
          rta_log(LOC, Er_No_Mem);
          rta_cmd.err = 1;
          free_row(rta_cmd.ptbl, pr);
          return;
        }
        **(int **) pd = (int) 0;
        break;
      case RTA_LONG:
        *(llong *) pd = (llong) 0;
        break;
      case RTA_PLONG:
        *(void **) pd = malloc(sizeof(llong));
        if (*(void **)pd == (void **) 0) {
          rta_log(LOC, Er_No_Mem);
          rta_cmd.err = 1;
          free_row(rta_cmd.ptbl, pr);
          return;
        }
        **(llong **) pd = (llong) 9;
        break;
      case RTA_PTR:
        /* Generic pointer.  Set it to null as init value */
        *(void **) pd = (void *) 0;
        break;
      case RTA_FLOAT:
        *(float *) pd = (float) 0.0;
        break;
      case RTA_PFLOAT:
        *(void **) pd = malloc(sizeof(float));
        if (*(void **)pd == (void **) 0) {
          rta_log(LOC, Er_No_Mem);
          rta_cmd.err = 1;
          free_row(rta_cmd.ptbl, pr);
          return;
        }
        **(float **) pd = (float) 0.0;
        break;

      case RTA_SHORT:
        *(short *) pd = (short) 0;
        break;
      case RTA_UCHAR:
        *(unsigned char *) pd = (unsigned char) 0;
        break;
      case RTA_DOUBLE:
        *(double *) pd = (double) 0.0;
        break;
    }
  }
  /* We have allocated memory for all of the row's pointer
     types.  From this point on we need to be careful to 
     delete this memory if anything goes wrong with the insert */

  /* Go through the values passed in and update the row. */
  for (cx = 0; cx < rta_cmd.ncols; cx++) {
    /* compute pointer to actual data */
    pd = (char *)pr + rta_cmd.pcol[cx]->offset;
    switch ((rta_cmd.pcol[cx])->type) {
      case RTA_STR:
        strncpy((char *) pd, rta_cmd.updvals[cx], rta_cmd.pcol[cx]->length);
        break;
      case RTA_PSTR:
        strncpy(*(char **) pd, rta_cmd.updvals[cx], rta_cmd.pcol[cx]->length);
        break;
      case RTA_INT:
        *((int *) pd) = rta_cmd.updints[cx];
        break;
      case RTA_SHORT:
        *((short *) pd) = rta_cmd.updints[cx];
        break;
      case RTA_UCHAR:
        *((unsigned char *) pd) = rta_cmd.updints[cx];
        break;
      case RTA_PINT:
        **((int **) pd) = rta_cmd.updints[cx];
        break;
      case RTA_LONG:
        *((llong *) pd) = rta_cmd.updlngs[cx];
        break;
      case RTA_PLONG:
        **((llong **) pd) = rta_cmd.updlngs[cx];
        break;
      case RTA_PTR:
        /* works only if INT and PTR are same size */
        *((int *) pd) = rta_cmd.updints[cx];
        break;
      case RTA_FLOAT:
        *((float *) pd) = rta_cmd.updflot[cx];
        break;
      case RTA_PFLOAT:
        **((float **) pd) = rta_cmd.updflot[cx];
        break;
      case RTA_DOUBLE:
        *((double *) pd) = rta_cmd.upddbl[cx];
        break;
    }
  }

  /* The row is complete.  Call the insert callback to attach it
     to the rest of the table structure.  On failure, free the
     row.  On success, call any write callbacks for the row. */
  rx = rta_cmd.ptbl->insertcb(rta_cmd.ptbl->name, rta_cmd.sqlcmd, pr);
  if (rx < 0) {
    free_row(rta_cmd.ptbl, pr);
    rta_send_error(LOC, E_BADINSERT, rta_cmd.ptbl->name);
    return;
  }

  /* Do all write callbacks after row is added to table */
  for (cx = 0; cx < rta_cmd.ptbl->ncol; cx++) {
    /* execute write callback (if defined) on row. callback will
       perform post processing on row and return zero on success */
    if (rta_cmd.ptbl->cols[cx].writecb) {
      if ((rta_cmd.ptbl->cols[cx].writecb) (rta_cmd.tbl,
          rta_cmd.ptbl->cols[cx].name, rta_cmd.sqlcmd, pr, rx, pr) != 0) {
        /* on error, send error message to user and delete row */
        rta_send_error(LOC, E_BADTRIG, rta_cmd.ptbl->cols[cx].name);
        rta_cmd.ptbl->deletecb(rta_cmd.ptbl->name, rta_cmd.sqlcmd, pr);
        return;
      }
    }
    if (rta_cmd.ptbl->cols[cx].flags & RTA_DISKSAVE)
      svt = 1;
  }

  /* Save the table to disk if needed */
  if (svt && rta_cmd.ptbl->savefile && strlen(rta_cmd.ptbl->savefile))
    rta_save(rta_cmd.ptbl, rta_cmd.ptbl->savefile);

  /* Send the INSERT complete message */
  *buf++ = 'C';
  tmark = buf;                  /* Save length location */
  buf += 4;
  ad_str(&buf, "INSERT", 6);
  n = sprintf(buf, " %d 1", rx);
  buf += n;
  *buf++ = 0x00;
  ad_int4(&tmark, (buf - tmark));

  *nbuf -= (int) (buf - startbuf);
}


/***************************************************************
 * free_row(): - Free any memory allocated as part of a row insert
 *
 * Input:        A pointer to the table definition
 *               A pointer to the row
 * Output:       None
 * Effects:      None
 ***************************************************************/
static void
free_row(RTA_TBLDEF *ptbl, void *pr)
{
  int cx;              /* column index */
  unsigned char * pd;  /* points the i-th column in the row */

  /* walk the column definitions delete any pointer types 
     with a valid pointer. */
  for (cx = 0; cx < ptbl->ncol; cx++) {
    pd = (unsigned char *)(pr + ptbl->cols[cx].offset);

    /* Switch on data type alloc/init field */
    switch ((ptbl->cols[cx]).type) {
      case RTA_PSTR:     /* fall through */
      case RTA_PINT:     /* fall through */
      case RTA_PLONG:    /* fall through */
      case RTA_PFLOAT:
        if (*pd)
          free(*(void **)pd);
        break;

      default:
        break;
    }
  }

  /* Now delete the row itself */
  free((char *) pr);
}

/***************************************************************
 * do_delete(): - Execute a DELETE statement against the DB.
 *
 * Input:        A buffer to store the output
 *               The number of free bytes in the buffer
 * Output:       The number of free bytes in the buffer
 * Effects:      Maybe lots.  DELETE on a table usually involves
 *               memory frees.  Look here first for a memory leak.
 ***************************************************************/
static void
do_delete(char *buf, int *nbuf)
{
  int      sr;         /* the Size of each Row in the table */
  int      rx;         /* Row indeX in for() loop */
  int      wx;         /* Where clause indeX in for loop */
  void    *pr;         /* Pointer to the row in the table/column */
  void    *newpr;      /* Pointer to the next row in the table/column */
  void    *pd;         /* Pointer to the Data in the table/column */
  llong cmp;           /* has actual relation of col and val */
  int      dor;        /* DO Row == 1 if we should delete row */
  char    *startbuf;   /* used to compute response length */
  int      n;          /* number of chars printed in sprintf() */
  int      nrd = 0;    /* =# rows deleted */
  int      svt = 0;    /* Save table if == 1 */
  int      cx;         /* Column indeX for looking for DISKSAVE cols */
  char    *tmark;      /* Address of D in "CDELETE" if success */

  startbuf = buf;

  /* We loop through all rows in the table in question applying the
     WHERE condition.  If a row matches we call the delete callback */
  sr = rta_cmd.ptbl->rowlen;
  rx = 0;
  if (rta_cmd.ptbl->iterator) 
    pr = (rta_cmd.ptbl->iterator) ((void *) NULL, rta_cmd.ptbl->it_info, rx);
  else
    pr = rta_cmd.ptbl->address;
  /* for each row ..... */
  while (pr) {
    dor = 1;
    for (wx = 0; wx < rta_cmd.nwhrcols; wx++) {
      /* The WHERE clause ...... execute read callback (if defined) on
         row * the call back is expected to fill in the data */
      if (rta_cmd.pwhr[wx]->readcb) {
        if ((rta_cmd.pwhr[wx]->readcb) (rta_cmd.tbl, rta_cmd.whrcols[wx],
            rta_cmd.sqlcmd, pr, rx) != 0) {
          rta_send_error(LOC, E_BADTRIG, rta_cmd.whrcols[wx]);
          return;
        }
      }

      /* compute pointer to actual data */
      pd = (char *)pr + rta_cmd.pwhr[wx]->offset;

      /* do comparison based on column data type */
      switch (rta_cmd.pwhr[wx]->type) {
        case RTA_STR:
          cmp = strncmp((char *) pd, rta_cmd.whrvals[wx],
			rta_cmd.pwhr[wx]->length);
          break;
        case RTA_PSTR:
          cmp = strcmp(*(char **) pd, rta_cmd.whrvals[wx]);
          break;
        case RTA_INT:
          cmp = *((int *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_SHORT:
          cmp = *((short *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_UCHAR:
          cmp = *((unsigned char *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_PINT:
          cmp = **((int **) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_LONG:
          cmp = *((llong *) pd) - rta_cmd.whrlngs[wx];
          break;
        case RTA_PLONG:
          cmp = **((llong **) pd) - rta_cmd.whrlngs[wx];
          break;
        case RTA_FLOAT:
          cmp = *((float *) pd) - rta_cmd.whrflot[wx];
          break;
        case RTA_PFLOAT:
          cmp = **((float **) pd) - rta_cmd.whrflot[wx];
          break;
        case RTA_PTR:
          cmp = *((int *) pd) - rta_cmd.whrints[wx];
          break;
        case RTA_DOUBLE:
          cmp = *((double *) pd) - rta_cmd.whrdbl[wx];
          break;
        default:
          cmp = 1;              /* assume no match */
          break;
      }
      if (!(((cmp == 0) && (rta_cmd.whrrel[wx] == RTA_EQ ||
              rta_cmd.whrrel[wx] == RTA_GE ||
              rta_cmd.whrrel[wx] == RTA_LE)) ||
          ((cmp != 0) && (rta_cmd.whrrel[wx] == RTA_NE)) ||
          ((cmp < 0) && (rta_cmd.whrrel[wx] == RTA_LE ||
              rta_cmd.whrrel[wx] == RTA_LT)) ||
          ((cmp > 0) && (rta_cmd.whrrel[wx] == RTA_GE ||
              rta_cmd.whrrel[wx] == RTA_GT)))) {
        dor = 0;
        break;
      }
    }

    /* In the next step we may delete the row (which frees the memory
       for it).  We'd better get the address of the _next_ row before
       we delete this one. */
    rx++;
    if (rta_cmd.ptbl->iterator)
      newpr = (rta_cmd.ptbl->iterator) (pr, rta_cmd.ptbl->it_info, rx);
    else {
      if (rx >= rta_cmd.ptbl->nrows)
        newpr = (void *) NULL;
      else
        newpr = (char *)rta_cmd.ptbl->address + (rx * sr);
    }


    if (dor && rta_cmd.offset)
      rta_cmd.offset--;
    else if (dor) {             /* DO Row */
      /* if we get here, we've passed the WHERE clause and the OFFSET.
         Check for LIMIT filtering */
      if (rta_cmd.limit <= 0) {
        break;
      }

      /* At this point we have a row which passed the * WHERE clause,
         is greater than OFFSET and less * than LIMIT. So delete it! */
      rta_cmd.ptbl->deletecb(rta_cmd.ptbl->name, rta_cmd.sqlcmd, pr);
      rta_cmd.limit--;       /* decrement row limit count */
      nrd++;
    }
    pr = newpr;
  }

  /* Save the table if any column is marked as DISKSAVE */
  for (cx = 0; cx < rta_cmd.ptbl->ncol; cx++) {
    if (rta_cmd.ptbl->cols[cx].flags & RTA_DISKSAVE)
      svt = 1;
  }
  if (svt && rta_cmd.ptbl->savefile && strlen(rta_cmd.ptbl->savefile))
    rta_save(rta_cmd.ptbl, rta_cmd.ptbl->savefile);

  /* Send the delete complete message */
  *buf++ = 'C';
  tmark = buf;                  /* Save length location */
  buf += 4;
  ad_str(&buf, "DELETE", 6);
  n = sprintf(buf, " %d", nrd); /* # rows affected */
  buf += n;
  *buf++ = 0x00;
  ad_int4(&tmark, (buf - tmark));

  *nbuf -= (int) (buf - startbuf);
}

/***************************************************************
 * ad_str(): - Add a string to the output buffer.  Includes a
 *             NULL to terminate the string.
 *
 * Input:        A **char to the buffer and the string and a length
 * Output:       void
 * Effects:      Increments the **char to point to the next
 *               available space in the buffer.
 ***************************************************************/
static void
ad_str(char **pbuf, char *instr, int count)
{
  strncpy(*pbuf, instr, count);
  *pbuf += count;
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
static void
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
static void
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
 * rta_log(): - Sends debug log messages to syslog() and stderr.
 *
 * Output:       void
 * Effects:      Increments the **char to point to the next
 *               available space in the buffer.
 ***************************************************************/
void
rta_log(char *fname,    /* error detected in file... */
  int linen,           /* error detected at line # */
  char *format, ...)
{                               /* printf format string */
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

  syslog(LOG_WARNING, format, fname, linen, s1, s2);

  /* Uncomment below for output to stderr */
  // fprintf(stderr, format, fname, linen, s1, s2);
  // fprintf(stderr, "\n");
}
