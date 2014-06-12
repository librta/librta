/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2004 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 *
 *  This file is derived in part from "FUSE: Filesystem in Userspace"
 *  which is Copyright (C) 2001  Miklos Szeredi (mszeredi@inf.bme.hu)
 **************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <limits.h>             /* for PATH_MAX */
#include <fuse.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "rta.h"
#include "do_sql.h"             /* for column print lengths */

extern TBLDEF *Tbl[];
extern int Ntbl;
extern COLDEF *Col[];
extern int Ncol;
extern struct RtaStat rtastat;
extern struct RtaDbg rtadbg;

        /* maximum # bytes in UPDATE response */
#define SQLRSPSIZE  (25)
        /* maximum # butes in pseudo SQL command (Getstr2 is 36) */
#define MXSQL       (36 + MXCOLNAME + MXTBLNAME + MX_INT_STRING)
        /* Document whether to use quotes or not */
#define QUOTES      (1)
#define NOQUOT      (0)

/* We save the mount point from the initialization so that we can
 * umount it when the program gets a signal */
static char save_mpt[PATH_MAX + 1]; /* mount point */

/* The FUSE package (File system in USEr space) uses a structure
 * to hold all of its internal state.  */
static struct fuse *fuse;

/* The file descriptor for the mounted virtual file system. */
static int fuse_fd;

/* The structure RTAFILE contains the parse of the file system 
 * path which is passed to all file IO routines.  The parsing
 * makes subsequent execution of the request easier. */
typedef struct
{
  char     tbl[MXTBLNAME + 1]; /* table name */
  char     col[MXCOLNAME + 1]; /* col name or "ROWS" */
  char     leaf[NAME_MAX + 1]; /* numeric row# */
  int      t;          /* Table ID if valid table, or -1 */
  int      c;          /* Column ID if valid column, or -1 */
  int      r;          /* -1 or valid row number */
  int      rt;         /* Request type: 0-7, see below for types */
} RTAFILE;

/* Forward references for internal routines */
static int rta_parse_path(RTAFILE *, const char *);
static int getcolsz(int);
static int getrwsz(int);
static int sprtcol(int, int, COLDEF *, char *, int, int);
static int prtsetrow(char **, RTAFILE *, char *, int);

/* Strings to build the UPDATE commands used in file writes
 * strings used to build SELECT command used for logging.  */
static char Setstr1[] = "UPDATE %s SET %s=";
static char Setstr2[] = " LIMIT 1 OFFSET %d";
static char Setstr3[] = "UPDATE %s  SET ";
static char Setstr4[] = "%s=%s";
static char Getstr1[] = "SELECT * FROM %s LIMIT 1 OFFSET %d";
static char Getstr2[] = "SELECT %s FROM %s LIMIT 1 OFFSET %d";

/** ************************************************************
 * rta_getattr():  Get the attributes of a file the RTA virtual
 * file system.  The path of the file is an encoded form of the
 * specific table, columns, and row.  We verify that the path 
 * is a valid request in that it is asking for a valid row of
 * a valid column of a valid table.  We set the mode of the
 * file based on read/write ability of the table or column.
 * We set the file size based on the printed size of the row
 * or column.
 * The stat include the modification time, the GID and the
 * UID.
 *
 * Input:        path   -- the path of the file to stat.
 *               stbuf  -- the stat structure to fill in.
 * Output:       Zero on success; a negative error code on failure.
 *               return(-ENOENT) if an invalid table/column/row1
 *               return(-EACCES) if read/write permission error
 ***************************************************************/
static int
rta_getattr(const char *path, struct stat *stbuf)
{
  RTAFILE  rf;
  int      c;          /* column index in a loop */
  char    *tbuf;       /* malloc'ed print buffer */
  int      out;        /* cumulative #bytes output by sprint */
  int      nr;         /* number of rows in table */
  void    *pr;         /* points to a table row */

  if (rta_parse_path(&rf, path) != RTA_SUCCESS)
    return (-ENOENT);

  memset(stbuf, 0, sizeof(struct stat));

  switch (rf.rt) {
    case 0:                    /* / */
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 2 + Ntbl; /* "." ".." and tables */
      break;

    case 1:                    /* /(table) */
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 2 + Tbl[rf.t]->ncol;
      break;

    case 2:                    /* /(table)/ROWS */
      stbuf->st_mode = S_IFDIR | 0555;
      /* If an iterator is defined we must 'hand count' the rows */
      if (Tbl[rf.t]->iterator) {
        nr = 0;
        pr =
          (Tbl[rf.t]->iterator) ((void *) NULL, Tbl[rf.t]->it_info, nr);
        while (pr) {
          nr++;
          pr = (Tbl[rf.t]->iterator) (pr, Tbl[rf.t]->it_info, nr);
        }
        stbuf->st_nlink = 2 + nr;
      }
      else
        stbuf->st_nlink = 2 + Tbl[rf.t]->nrows;
      break;

    case 3:                    /* /(table)/ROWS/(row#) */
      stbuf->st_nlink = 1;
      /* Get size by printing row to temp buf */
      /* default is maximum size */
      stbuf->st_size = getrwsz(rf.t);
      tbuf = malloc(stbuf->st_size + 2);
      if (tbuf == (char *) NULL) {
        rtastat.nsyserr++;
        if (rtadbg.syserr)
          rtalog(LOC, Er_No_Mem);
        break;
      }
      out = 0;
      for (c = 0; c < Tbl[rf.t]->ncol; c++) {
        /* add comma */
        if (c != 0) {
          tbuf[out++] = ',';
          tbuf[out++] = ' ';
        }
        out += sprtcol(rf.t, rf.r, &(Tbl[rf.t]->cols[c]),
          (tbuf + out), stbuf->st_size - out, QUOTES);
      }
      stbuf->st_size = out + 1; /* 1 for newline */
      if (tbuf)
        free(tbuf);
      /* mode = RO if all columns are RO */
      stbuf->st_mode = S_IFREG | 0444;
      for (c = 0; c < Tbl[rf.t]->ncol; c++) {
        if (!(Tbl[rf.t]->cols[c].flags & RTA_READONLY)) {
          stbuf->st_mode |= 0644;
          break;
        }
      }
      break;

    case 4:                    /* /(table)/(column) */
      stbuf->st_mode = S_IFDIR | 0555;
      if (Tbl[rf.t]->iterator) {
        nr = 0;
        pr =
          (Tbl[rf.t]->iterator) ((void *) NULL, Tbl[rf.t]->it_info, nr);
        while (pr) {
          nr++;
          pr = (Tbl[rf.t]->iterator) (pr, Tbl[rf.t]->it_info, nr);
        }
        stbuf->st_nlink = 2 + nr;
      }
      else
        stbuf->st_nlink = 2 + Tbl[rf.t]->nrows;
      break;

    case 5:                    /* /(table)/(column)/(row#) */
      if (Col[rf.c]->flags & RTA_READONLY)
        stbuf->st_mode = S_IFREG | 0444;
      else
        stbuf->st_mode = S_IFREG | 0644;
      stbuf->st_nlink = 1;
      /* Get size by printing column to temp buf */
      /* default is maximum size */
      stbuf->st_size = getcolsz(rf.c);
      tbuf = malloc(stbuf->st_size + 2);
      if (tbuf == (char *) NULL) {
        rtastat.nsyserr++;
        if (rtadbg.syserr)
          rtalog(LOC, Er_No_Mem);
        break;
      }
      out =
        sprtcol(rf.t, rf.r, Col[rf.c], tbuf, stbuf->st_size, NOQUOT);
      stbuf->st_size = out + 1; /* +1 for newline */
      if (tbuf)
        free(tbuf);
      break;

    default:
      return (-ENOENT);
      break;
  }
  (void) time((time_t *) & (stbuf->st_atime)); /* last access time */
  stbuf->st_mtime = stbuf->st_atime; /* Time of last mod */
  stbuf->st_ctime = stbuf->st_atime; /* last status change.  */
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();

  return (0);
}

/** ************************************************************
 * rta_getdir():  Handle a getdir() request for a specific path.
 * For a request on the '/', the mount point, we return a list
 * all the tables.  For a request on a table name, we return the
 * word "ROWS" and a list of all the column names.  For either
 * "ROWS" or a column name, we return a list of the rows numbers
 * with the rows numbered "0000" up the then maximum row number
 * minus 1.
 *   A consistency check verifies that any column specified is in
 * the right table.
 *
 * Input:        path   -- the path of the directory in question
 *               h      -- a FUSE directory entry
 *               filler -- subroutine to add a filler into h.
 * Output:       Zero on success; a negative error code on failure.
 *               return(-ENOENT) if an invalid table/column/row
 ***************************************************************/
static int
rta_getdir(const char *path, fuse_dirh_t h, fuse_dirfil_t filler)
{
  int      i;
  RTAFILE  rf;
  char     rowstr[15]; /* 15=safe value for #digits in row number */
  int      nr;         /* number of rows in table */
  void    *pr;         /* pointer to a row */

  if (rta_parse_path(&rf, path) != RTA_SUCCESS)
    return (-ENOENT);

  switch (rf.rt) {
    case 0:                    /* / */
      filler(h, ".", 0);
      filler(h, "..", 0);
      for (i = 0; i < Ntbl; i++) {
        filler(h, Tbl[i]->name, 0);
      }
      break;

    case 1:                    /* /(table) */
      filler(h, ".", 0);
      filler(h, "..", 0);
      filler(h, "ROWS", 0);
      for (i = 0; i < Tbl[rf.t]->ncol; i++) {
        filler(h, Tbl[rf.t]->cols[i].name, 0);
      }
      break;

    case 2:                    /* /(table)/ROWS */
      filler(h, ".", 0);
      filler(h, "..", 0);
      nr = Tbl[rf.t]->nrows;
      /* Manually count rows if an iterator is defined */
      if (Tbl[rf.t]->iterator) {
        nr = 0;
        pr =
          (Tbl[rf.t]->iterator) ((void *) NULL, Tbl[rf.t]->it_info, nr);
        while (pr) {
          nr++;
          pr = (Tbl[rf.t]->iterator) (pr, Tbl[rf.t]->it_info, nr);
        }
      }
      for (i = 0; i < nr; i++) {
        sprintf(rowstr, "%04d", i);
        filler(h, rowstr, 0);
      }
      break;

    case 3:                    /* /(table)/ROWS/(row#) */
      return (-ENOENT);
      break;

    case 4:                    /* /(table)/(column) */
      filler(h, ".", 0);
      filler(h, "..", 0);
      nr = Tbl[rf.t]->nrows;
      /* Manually count rows if an iterator is defined */
      if (Tbl[rf.t]->iterator) {
        nr = 0;
        pr =
          (Tbl[rf.t]->iterator) ((void *) NULL, Tbl[rf.t]->it_info, nr);
        while (pr) {
          nr++;
          pr = (Tbl[rf.t]->iterator) (pr, Tbl[rf.t]->it_info, nr);
        }
      }
      for (i = 0; i < nr; i++) {
        sprintf(rowstr, "%04d", i);
        filler(h, rowstr, 0);
      }
      break;

    case 5:                    /* /(table)/(column)/(row#) */
      return (-ENOENT);
      break;

    default:
      return (-ENOENT);
      break;
  }

  return (0);
}

/** ************************************************************
 * rta_open():  Handle an open() request for a specific file.
 * The path of the file is an encoded form of the specific table,
 * columns, and row.  We verify that the path is a valid request
 * in that it is asking for a valid row of a valid column of a
 * valid table.  Also verifies that the read/write flag is set
 * correctly for the row or column.  That is, we return an error
 * the open is WRONLY on what is a read-only column or table.
 * Return zero if everything is OK and an error if there is a
 * problem.
 *
 * Input:        path   -- the path of the file to read.
 *               flags  -- the read/write type of access
 * Output:       Zero on success; a negative error code on failure.
 *               return(-ENOENT) if an invalid table/column/row
 *               return(-EACCES) if read/write permission error
 ***************************************************************/
static int
rta_open(const char *path, int flags)
{
  RTAFILE  rf;
  int      c;          /* column index in a loop */

  if (rta_parse_path(&rf, path) != RTA_SUCCESS)
    return (-ENOENT);

  switch (rf.rt) {
    case 0:                    /* / */
    case 1:                    /* /(table) */
    case 2:                    /* /(table)/ROWS */
    case 4:                    /* /(table)/(column) */
      if ((flags & O_WRONLY) || (flags & O_RDWR))
        return (-EACCES);
      else
        return (0);
      break;

    case 3:                    /* /(table)/ROWS/(row#) */
      /* return EACCES if all cols are RO and a WR open */
      if ((flags & O_WRONLY) || (flags & O_RDWR)) {
        for (c = 0; c < Tbl[rf.t]->ncol; c++) {
          if (!(Tbl[rf.t]->cols[c].flags & RTA_READONLY))
            return (0);
        }
        return (-EACCES);
      }
      return (0);
      break;

    case 5:                    /* /(table)/(column)/(row#) */
      if (((flags & O_WRONLY) || (flags & O_RDWR))
        && (Col[rf.c]->flags & RTA_READONLY))
        return (-EACCES);
      else
        return (0);
      break;

    default:
      return (-ENOENT);
      break;
  }
}

/** ************************************************************
 * rta_read():  Handle a read request for a specific file.
 * The path of the file is an encoded form of the specific table,
 * columns, and row.  
 * Our implementation of a virtual file system has no concept of
 * file descriptor, so a read that spans multiple buffers (ie
 * requires multiple read() calls) can not guarantee consistency.
 * Your minimum read buffer size should be at least the maximum
 * size of the row or column that you are reading.
 *
 * Input:        path   -- the path of the file to read.
 *               buf    -- where to store the output
 *               size   -- the number of bytes in buf
 *               offset -- offset from the start of the file.
 * Output:       The number of bytes placed in buf.  A negative
 *               return value indicates an error.
 * Effects:      Column read callbacks are invoked if needed.
 ***************************************************************/
static int
rta_read(const char *path, char *buf, size_t size, off_t offset)
{
  RTAFILE  rf;
  int      out = 0;    /* number of bytes added to buf */
  int      c;          /* column index */
  int      i, j;       /* temp loop counters */
  char    *tbuf;       /* holds print string */
  int      prtsize;    /* maximum number char in printed line */
  char     sql[MXSQL]; /* pseudo SQL command for logging */

  if (rta_parse_path(&rf, path) != RTA_SUCCESS)
    return (-ENOENT);

  switch (rf.rt) {
    case 0:                    /* / */
      break;

    case 1:                    /* /(table) */
      break;

    case 2:                    /* /(table)/ROWS */
      break;

    case 3:                    /* /(table)/ROWS/(row#) */
      /* Print row to temp buf */
      prtsize = getrwsz(rf.t);
      tbuf = malloc(prtsize + 2);
      if (tbuf == (char *) NULL) {
        rtastat.nsyserr++;
        if (rtadbg.syserr)
          rtalog(LOC, Er_No_Mem);
        out = 0;
        break;
      }
      out = 0;
      for (c = 0; c < Tbl[rf.t]->ncol; c++) {
        /* add comma */
        if (c != 0) {
          tbuf[out++] = ',';
          tbuf[out++] = ' ';
        }
        out += sprtcol(rf.t, rf.r, &(Tbl[rf.t]->cols[c]),
          (tbuf + out), (int) size, QUOTES);
      }
      /* add newline */
      tbuf[out++] = '\n';
      tbuf[out] = (char) NULL;
      /* copy temp buf to buf using offset */
      if (offset >= out) {
        out = 0;                /* offset is past string length, return 
                                   nothing */
      }
      else {
        for (j = 0, i = offset; (i < out && i < size); j++, i++) {
          buf[j] = tbuf[i];
        }
      }
      free(tbuf);
      /* If trace is on, build a pseudo-SQL command and log it */
      if (rtadbg.trace) {
        (void) sprintf(sql, Getstr1, rf.tbl, rf.r);
        rtalog(LOC, Er_Trace_SQL, sql, "1");
      }
      rtastat.nselect++;        /* increment stats counter */
      break;

    case 4:                    /* /(table)/(column) */
      break;

    case 5:                    /* /(table)/(column)/(row#) */
      /* Print column to temp buf */
      prtsize = getcolsz(rf.c);
      tbuf = malloc(prtsize + 2);
      if (tbuf == (char *) NULL) {
        rtastat.nsyserr++;
        if (rtadbg.syserr)
          rtalog(LOC, Er_No_Mem);
        out = 0;
        break;
      }
      out = sprtcol(rf.t, rf.r, Col[rf.c], tbuf, prtsize, NOQUOT);
      /* add newline */
      tbuf[out++] = '\n';
      tbuf[out] = (char) NULL;
      /* copy temp buf to buf using offset */
      if (offset >= out) {
        out = 0;                /* offset is past string length, return 
                                   nothing */
      }
      else {
        for (j = 0, i = offset; (i < out && i < size); j++, i++) {
          buf[j] = tbuf[i];
        }
      }
      free(tbuf);
      /* If trace is on, build a pseudo-SQL command and log it */
      if (rtadbg.trace) {
        (void) sprintf(sql, Getstr2, rf.col, rf.tbl, rf.r);
        rtalog(LOC, Er_Trace_SQL, sql, "1");
      }
      rtastat.nselect++;        /* increment stats counter */
      break;

    default:
      return (-ENOENT);
      break;
  }

  return (out);
}

/** ************************************************************
 * rta_write():  Handle a write request to a specific file.
 * The path of the file is an encoded form of the specific table,
 * columns, and row.  
 * Our implementation of a virtual file system has no concept of
 * file descriptor, and since we want to maintain consistency, we
 * require that all data fit into a single write buffer.  So, if
 * the write offset is not a zero we return an error and warn
 * that table consistency may have been compromised.
 *
 * Input:        path   -- the path of the file to write. It is
 *                    also the table, columns, and row to update
 *               buf    -- has the characters to write
 *               size   -- the number of bytes to write
 *               offset -- offset from the start of the file, an
 *                         error if this is not zero.
 * Output:       The number of bytes written.  Should always be 
 *               equal to 'size'.
 * Effects:      Column write callbacks are invoked if needed.
 ***************************************************************/
static int
rta_write(const char *path, const char *buf, size_t size, off_t offset)
{
  RTAFILE  rf;
  int      out = 0;    /* number of bytes added to buf */
  int      j;          /* temp loop counter */
  char    *tbuf;       /* holds print string */
  int      sqlsize;    /* maximum number char in SQL request */
  char     obuf[SQLRSPSIZE + 1];
  char     quot;       /* quote terminator for strings */

  if (rta_parse_path(&rf, path) != RTA_SUCCESS)
    return (-ENOENT);

  switch (rf.rt) {
    case 0:                    /* / */
      break;

    case 1:                    /* /(table) */
      break;

    case 2:                    /* /(table)/ROWS */
      break;

    case 3:                    /* /(table)/ROWS/(row#) */
      if (offset != 0) {
        rtastat.nrtaerr++;
        if (rtadbg.rtaerr)
          rtalog(LOC, Er_No_Space);
        return (-EFBIG);        /* report error */
      }
      out = prtsetrow(&tbuf, &rf, (char *) buf, size);
      if (out)                  /* any errors? */
        return (out);
      j = SQLRSPSIZE;
      SQL_string(tbuf, obuf, &j);
      free(tbuf);
      if (!strncmp(obuf, "EERROR", 6))
        return (-EIO);          /* report error */
      out = (int) size;         /* say we wrote all bytes */
      break;

    case 4:                    /* /(table)/(column) */
      break;

    case 5:                    /* /(table)/(column)/(row#) */
      if (offset != 0) {
        rtastat.nrtaerr++;
        if (rtadbg.rtaerr)
          rtalog(LOC, Er_No_Space);
        return (-EFBIG);
      }
      /* to write a single column, we build an SQL string */
      sqlsize = (int) size + strlen(Setstr1) + strlen(Setstr2)
        + MXTBLNAME + MXCOLNAME + 1;
      tbuf = malloc(sqlsize);
      if (tbuf == (char *) NULL) {
        rtastat.nsyserr++;
        if (rtadbg.syserr)
          rtalog(LOC, Er_No_Mem);
        return (-EIO);          /* say we wrote nothing */
        break;
      }
      out = sprintf(tbuf, Setstr1, rf.tbl, rf.col);
      /* Add single or double quotes around value */
      quot = (memchr(buf, '"', (int) size)) ? '\'' : '"';
      tbuf[out++] = quot;
      memcpy(tbuf + out, buf, (int) size);
      out += size;
      /* ignore trailing \n if present */
      if (tbuf[out - 1] == '\n')
        out--;
      tbuf[out++] = quot;
      (void) sprintf(tbuf + out, Setstr2, rf.r);
      j = SQLRSPSIZE;
      /* execute the UPDATE command */
      SQL_string(tbuf, obuf, &j);
      free(tbuf);
      if (!strncmp(obuf, "EERROR", 6))
        return (-EIO);          /* say we wrote nothing */
      out = (int) size;         /* say we wrote all bytes */
      break;

    default:
      /* Should not get here */
      return (-ENOENT);
      break;
  }

  return (out);
}

/** ************************************************************
 * rta_truncate():  Null file operation to truncate a file.
 *
 * Input:       (void)
 * Output:      (void)
 **************************************************************/
static int
rta_truncate(const char *path, off_t size)
{
  return 0;
}

static struct fuse_operations rta_oper = {
getattr:rta_getattr,
readlink:NULL,
getdir:rta_getdir,
mknod:NULL,
mkdir:NULL,
symlink:NULL,
unlink:NULL,
rmdir:NULL,
rename:NULL,
link:NULL,
chmod:NULL,
chown:NULL,
truncate:rta_truncate,
utime:NULL,
open:rta_open,
read:rta_read,
write:rta_write,
statfs:NULL,
release:NULL
};

/** ************************************************************
 * exit_handler():  Exit handler to try to unmount the virtual
 * file system.
 *
 * Input:       (void)
 * Output:      (void)
 **************************************************************/
static void
exit_handler()
{
  if (fuse != NULL) {
    close(fuse_fd);
    fuse_unmount(save_mpt);
    exit(0);
  }
}

/** ************************************************************
 * set_signal_handlers():  Install an exit handler to try to
 * unmount the virtual file system.
 *
 * Input:       (void)
 * Output:      (void)
 **************************************************************/
static void
set_signal_handlers()
{
  struct sigaction sa;

  sa.sa_handler = exit_handler;
  sigemptyset(&(sa.sa_mask));
  sa.sa_flags = 0;

  if (sigaction(SIGHUP, &sa, NULL) == -1 ||
    sigaction(SIGINT, &sa, NULL) == -1 ||
    sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("Cannot set exit signal handlers");
    exit(1);
  }

  sa.sa_handler = SIG_IGN;

  if (sigaction(SIGPIPE, &sa, NULL) == -1) {
    perror("Cannot set ignored signals");
    exit(1);
  }
}

/** ************************************************************
 * rtafs_init():  - Initialize the virtual file system interface
 * to RTA.  The single input parameter is the mount point for
 * the VFS.  The return value is the file descriptor on success.
 * On failure, a -1 is returned and errno is set.
 * An important SIDE EFFECT is the signal handlers for SIGHUP,
 * SIGINT, and SIGTERM are set.  The signal handler tries to 
 * unmount the virtual file system.
 * 
 * Input:  char *mountpoint - desired mount point
 *
 * Return: int fd        - file descriptor on success.
 **************************************************************/
int
rtafs_init(char *mountpoint)
{
  /* Set signal handler so we can unmount at exit */
  strncpy(save_mpt, mountpoint, PATH_MAX);
  set_signal_handlers();

  fuse_fd = fuse_mount(save_mpt, NULL);
  if (fuse_fd != -1)
    fuse = fuse_new(fuse_fd, 0, &rta_oper);

  return (fuse_fd);
}

/** ************************************************************
 * do_rtafs():  - Handle all of the actual virtual file system
 * IO.  This routine has all of the hooks to the user-space
 * implementations of all of the common file IO operations.
 * 
 * Input:  (none)
 *
 * Return: (none)
 **************************************************************/
void
do_rtafs()
{
  struct fuse_cmd *cmd;

  cmd = __fuse_read_cmd(fuse);
  if (cmd != NULL)
    __fuse_process_cmd(fuse, cmd);
}

/** ************************************************************
 * rta_parse_path(): - Given a path in an RTA file system, parse
 * the path into table, column, and row number as needed. Check
 * consistency of table name, column name, and row number if
 * possible.
 *
 * Input:  RTAFILE structure pointer and the path to parse
 * Output: RTA_SUCCESS   - Parse successful and consistent
 *         RTA_ERROR     - The passed path is not a valid path
 *                         in the current RTA table list.
 *                         A syslog error message describes the
 *                         problem
 **************************************************************/
static int
rta_parse_path(RTAFILE * pfs, const char *path)
{
  /* This routine parses and validates a path in a RTA-FS */
  /* file system.  Acceptable formats for path include ...  */
  /* "rt" Path */
  /* 0 / */
  /* 1 /(table) */
  /* 1 /(table)/ */
  /* 2 /(table)/ROWS */
  /* 2 /(table)/ROWS/ */
  /* 3 /(table)/ROWS/(row#) */
  /* 4 /(table)/(column) */
  /* 4 /(table)/(column)/ */
  /* 5 /(table)/(column)/(row#) */
  /* */
  /* We return as soon as we detect an error.  */

  int      i;          /* We use i as the index into the path string */
  int      j;          /* loop counter */
  int      nr;         /* number of rows in table */
  void    *pr;         /* pointer to a row */

  /* Init the structure to "/" */
  pfs->tbl[0] = (char) NULL;
  pfs->col[0] = (char) NULL;
  pfs->leaf[0] = (char) NULL;
  pfs->t = -1;
  pfs->c = -1;
  pfs->r = -1;

  /* The first character of the path should be a '/' */
  i = 0;
  if (path[i] != '/')
    return (RTA_ERROR);
  i++;

  /* skip over any extra '/' characters */
  while (path[i] == '/')
    i++;

  /* if next char is null, we have just '/' as path */
  if (path[i] == (char) NULL) {
    pfs->rt = 0;
    return (RTA_SUCCESS);
  }

  /* Not '/' or null, so must be a table name */
  j = 0;
  while (path[i] != '/' && path[i] != (char) NULL && j < MXTBLNAME) {
    pfs->tbl[j] = path[i];
    j++;
    i++;
  }
  pfs->tbl[j] = (char) NULL;

  /* OK, we have a table name, look up table ID */
  for (j = 0; j < Ntbl; j++) {
    if (!strncmp(pfs->tbl, Tbl[j]->name, MXTBLNAME))
      break;
  }
  if (j == Ntbl)
    return (RTA_ERROR);         /* no table match */
  pfs->t = j;                   /* save index into Tbl[] */

  /* Skip over any '/' characters */
  while (path[i] == '/')
    i++;

  /* if next is null, we have '/(table)' as path */
  if (path[i] == (char) NULL) {
    pfs->rt = 1;
    return (RTA_SUCCESS);
  }

  /* To get here, must be a column name or "ROWS" */
  /* Put column name in structure */
  j = 0;
  while (path[i] != '/' && path[i] != (char) NULL && j < MXCOLNAME) {
    pfs->col[j] = path[i];
    j++;
    i++;
  }
  pfs->col[j] = (char) NULL;

  /* Look for ..../ROWS/(row#) */
  if (!strncmp(pfs->col, "ROWS", MXCOLNAME)) {
    /* In a ROWS/(row#) ptype */
    /* skip over '/' characters */
    while (path[i] == '/')
      i++;

    /* if null, path must be /table/ROWS/ */
    if (path[i] == (char) NULL) {
      pfs->rt = 2;
      return (RTA_SUCCESS);
    }

    /* rest of path is row number */
    strncpy(pfs->leaf, &path[i], NAME_MAX);
    nr = Tbl[pfs->t]->nrows;
    /* Manually count rows if an iterator is defined */
    if (Tbl[pfs->t]->iterator) {
      nr = 0;
      pr =
        (Tbl[pfs->t]->iterator) ((void *) NULL, Tbl[pfs->t]->it_info,
        nr);
      while (pr) {
        nr++;
        pr = (Tbl[pfs->t]->iterator) (pr, Tbl[pfs->t]->it_info, nr);
      }
    }
    if ((sscanf(pfs->leaf, "%d", &(pfs->r)) != 1) || (pfs->r >= nr))
      return (RTA_ERROR);
    else {
      pfs->rt = 3;
      return (RTA_SUCCESS);
    }
  }

  /* OK, we have a col, and it's not ROWS */
  /* Look up col ID, Only look at cols for found table */
  for (j = 0; j < Ncol; j++) {
    if (!strncmp(pfs->col, Col[j]->name, MXCOLNAME) &&
      (!strncmp(Col[j]->table, pfs->tbl, MXTBLNAME)))
      break;
  }
  if (j == Ncol)
    return (RTA_ERROR);         /* not a valid column */

  pfs->c = j;                   /* save index into Col[] */

  /* Skip over any '/' characters */
  while (path[i] == '/')
    i++;

  /* Look for null to terminate /table/column */
  if (path[i] == (char) NULL) {
    pfs->rt = 4;
    return (RTA_SUCCESS);
  }

  /* We have '/(table)/(column)/xxx'.  So we need a row #. */
  strncpy(pfs->leaf, &path[i], NAME_MAX);

  nr = Tbl[pfs->t]->nrows;
  /* Manually count rows if an iterator is defined */
  if (Tbl[pfs->t]->iterator) {
    nr = 0;
    pr =
      (Tbl[pfs->t]->iterator) ((void *) NULL, Tbl[pfs->t]->it_info, nr);
    while (pr) {
      nr++;
      pr = (Tbl[pfs->t]->iterator) (pr, Tbl[pfs->t]->it_info, nr);
    }
  }
  if ((sscanf(pfs->leaf, "%d", &(pfs->r)) != 1) || (pfs->r >= nr))
    return (RTA_ERROR);
  else {
    pfs->rt = 5;
    return (RTA_SUCCESS);
  }
}

/** ************************************************************
 * getrwsz():  Get the number of bytes in the string which
 * we get if print all columns of a table row with each
 * column taking its maximum length.
 * possible.
 *
 * Input:  t:            - Table index into Tbl
 * Output:               - Number of bytes in print string
 **************************************************************/
static int
getrwsz(int t)
{
  int      sum = 0;    /* return value = #bytes */
  int      c;          /* column index in table */

  for (c = 0; c < Tbl[t]->ncol; c++) {
    /* Switch on column type to get print string length */
    switch (Tbl[t]->cols[c].type) {
      case RTA_STR:
      case RTA_PSTR:
        sum = sum + Tbl[t]->cols[c].length;
        break;

      case RTA_PTR:
      case RTA_INT:
      case RTA_PINT:
        sum = sum + MX_INT_STRING;
        break;

      case RTA_LONG:
      case RTA_PLONG:
        sum = sum + MX_LONG_STRING;
        break;

      case RTA_FLOAT:
      case RTA_PFLOAT:
        sum = sum + MX_FLOT_STRING;
        break;

      default:
        /* Should never hit default */
        break;
    }
    sum = sum + 2;              /* for the comma and space */
  }
  return (sum);
}

/** ************************************************************
 * prtsetrow():  Convert a file write of a row file into an SQL
 * command.  Remove read-only columns.   Note that strings can
 * be deliminated with either a single quote or a double quote.
 *
 * Input:  c:            - Column index into Col
                prtsetrow(tbuf, &rf, buf, size);
 * Output:               - Number of bytes in print string
 **************************************************************/
static int
prtsetrow(char **xbuf, RTAFILE * prf, char *buf, int size)
{
  char    *pcb;        /* Pointer to a Copy of Buf */
  char    *tbuf;       /* Where we put the SQL string */
  int      out;        /* Number of characters in SQL string */
  int      c;          /* column index */
  int      i;          /* temp loop counters */
  int      sqlsize;    /* maximum number char in SQL request */
  char     quot;       /* the quote character for a string */
  COLDEF  *pc;         /* pointer to current column definition */
  int      didacolumn = 0; /* flag to add commas in SQL cmd */
  char    *flds[MX_COL]; /* array of ptrs to fields */

  /* Buf contains the ASCII text to represent a row of data.  */
  /* We buf into an allocated string and replace the commas */
  /* separating the fields with nulls.  We record the start */
  /* of each field in the flds array.  Of course we detect */
  /* single and double quotes around strings and ignore any */
  /* commas in a string.  Building a UPDATE SQL command is */
  /* easy with flds filled in.  */

  /* allocate memory for the copy of buf.  +1 for a null */
  pcb = malloc(size + 1);
  if (pcb == (char *) NULL) {
    rtastat.nsyserr++;
    if (rtadbg.syserr)
      rtalog(LOC, Er_No_Mem);
    return (-EIO);              /* report error */
  }

  /* copy buf into pcb and add null */
  memcpy(pcb, buf, size);
  pcb[size] = (char) NULL;

  /* Walk pcb recording the start of fields are replacing */
  /* with NULLs */
  i = 0;
  for (c = 0; c < Tbl[prf->t]->ncol; c++) {
    /* record start of new field */
    flds[c] = &(pcb[i]);

    /* detect error of no string */
    if (i >= size) {
      /* BOB: log this? */
      free(pcb);
      return (-EIO);
    }

    /* The columns are arranged in the order they appear in */
    /* the COLDEF table in the TBLDEF.  Scan to end of field */
    pc = &(Tbl[prf->t]->cols[c]);
    if ((pc->type == RTA_STR) || (pc->type == RTA_PSTR)) {
      /* We have a string type.  Detect quote type, ' or ". */
      while ((pcb[i] != '\'') && (pcb[i] != '"') &&
        (pcb[i] != (char) NULL))
        i++;
      quot = pcb[i];
      i++;
      while ((pcb[i] != quot) && (i < size))
        i++;
    }
    /* Find comma or end */
    while ((pcb[i] != ',') && (pcb[i] != (char) NULL))
      i++;
    pcb[i] = (char) NULL;

    /* point to next character in string */
    i++;
  }

  /* At this point we have an array of pointers to the */
  /* start of each field.  Now allocate enough memory */
  /* to hold the SQL command. */
  sqlsize =
    1 + (int) size + strlen(Setstr2) + strlen(Setstr3) + MXTBLNAME +
    Tbl[prf->t]->ncol * (2 + MXCOLNAME + strlen(Setstr4));
  tbuf = malloc(sqlsize);
  if (tbuf == (char *) NULL) {
    free(pcb);
    rtastat.nsyserr++;
    if (rtadbg.syserr)
      rtalog(LOC, Er_No_Mem);
    return (-EIO);              /* report error */
  }

  /* The columns are arranged in the order they appear in */
  /* the COLDEF table in the TBLDEF.  Print UPDATE command */
  /* Add "UPDATE table SET" */
  out = sprintf(tbuf, Setstr3, prf->tbl);

  /* Now add each writable column */
  for (c = 0; c < Tbl[prf->t]->ncol; c++) {
    pc = &(Tbl[prf->t]->cols[c]);
    if (pc->flags & RTA_READONLY)
      continue;                 /* No read-only columns */
    if (didacolumn)
      out += sprintf(tbuf + out, ", ");
    out += sprintf(tbuf + out, Setstr4, pc->name, flds[c]);
    didacolumn = 1;
  }
  /* Add "LIMIT 1 OFFSET n" */
  (void) sprintf(tbuf + out, Setstr2, prf->r);

  /* normal exit returns 0 */
  *xbuf = tbuf;
  free(pcb);
  return (0);
}

/** ************************************************************
 * getcolsz():  Get the number of bytes in the string which
 * we get if print the specified column using its maximum 
 * length.
 *
 * Input:  c:            - Column index into Col
 * Output:               - Number of bytes in print string
 **************************************************************/
static int
getcolsz(int c)
{
  switch (Col[c]->type) {
    case RTA_STR:
    case RTA_PSTR:
      return (Col[c]->length);
      break;

    case RTA_PTR:
    case RTA_INT:
    case RTA_PINT:
      return (MX_INT_STRING);
      break;

    case RTA_LONG:
    case RTA_PLONG:
      return (MX_LONG_STRING);
      break;

    case RTA_FLOAT:
    case RTA_PFLOAT:
      return (MX_FLOT_STRING);
      break;

    default:
      /* Should never hit default */
      return (0);
      break;
  }
}

/** ************************************************************
 * sprtcol(): Print a row/column from a table.  Invoke read 
 * callback if needed.  
 *
 * Input:  t:            - Table index into Tbl
 *         r:            - Row to print
 *         pc:           - Column pointer to column to print
 *         buf:          - Where to print the value
 *         nbuf;         - Free space available at buf
 *         quot;         - ==1 to put quotes around strings
 * Output:               - Number of bytes added to buf
 **************************************************************/
static int
sprtcol(int t, int r, COLDEF *pc, char *buf, int nbuf, int quot)
{
  void    *pr;         /* Pointer to the row in the column */
  void    *pd;         /* Pointer to the Data in the table/column */
  int      out;        /* number of bytes added to buf */
  int      nr;         /* number of rows in table */

  /* compute pointer to actual data */
  pr = Tbl[t]->address + (r * Tbl[t]->rowlen);

  /* Iterate down to row if an iterator is defined */
  if (Tbl[t]->iterator) {
    nr = 0;
    pr = (Tbl[t]->iterator) ((void *) NULL, Tbl[t]->it_info, nr);
    while (pr && nr < r) {
      nr++;
      pr = (Tbl[t]->iterator) (pr, Tbl[t]->it_info, nr);
    }
  }

  pd = pr + pc->offset;

  /* execute read callback (if defined) on row */
  /* the call back is expected to fill in the data */
  if (pc->readcb) {
    (pc->readcb) (Tbl[t]->name, pc->name, "", pr, r);
  }

  /* Print is based on data type.  */
  switch (pc->type) {
    case RTA_STR:
      if (quot == NOQUOT)
        out = sprintf(buf, "%s", (char *) pd);
      else {
        if (memchr((char *) pd, '"', pc->length))
          out = sprintf(buf, "\'%s\'", (char *) pd);
        else
          out = sprintf(buf, "\"%s\"", (char *) pd);
      }
      break;

    case RTA_PSTR:
      if (quot == NOQUOT)
        out = sprintf(buf, "%s", *(char **) pd);
      else {
        if (memchr((char *) pd, '"', pc->length))
          out = sprintf(buf, "\'%s\'", *(char **) pd);
        else
          out = sprintf(buf, "\"%s\"", *(char **) pd);
      }
      break;

    case RTA_INT:
      out = sprintf(buf, "%d", *((int *) pd));
      break;

    case RTA_PINT:
      out = sprintf(buf, "%d", **((int **) pd));
      break;

    case RTA_LONG:
      out = sprintf(buf, "%lld", *((long long *) pd));
      break;

    case RTA_PLONG:
      out = sprintf(buf, "%lld", **((long long **) pd));
      break;

    case RTA_PTR:
      /* works only if INT and PTR are same size */
      out = sprintf(buf, "%d", *((int *) pd));
      break;

    case RTA_FLOAT:
      out = sprintf(buf, "%f", *((float *) pd));
      break;

    case RTA_PFLOAT:
      out = sprintf(buf, "%f", **((float **) pd));
      break;
  }
  return (out);
}
