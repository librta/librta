/***************************************************************************
 *                        select.c
 *  copyright            : (C) 2004 by Bob Smith
 *
 *   This file has the select() loop code for the empty daemon.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   THE AUTHOR RENOUNCES ALL COPYRIGHT TO THIS WORK.                      *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it for redistribution as you see fit.  Specifically, you may use this *
 *   as the basis of your software and you need not make public your       *
 *   programs or other work derived from this program.  You may remove     *
 *   the above copyright in any derived work.                              *
 *                                                                         *
 *   However, please let the authors of this program know if you find bugs *
 *   or security problems in the program.                                  *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *  Change Log:                                                            *
 *     2004 Mar 5: Initial release                                         *
 ***************************************************************************/


static char const Version_select_c[] = "$Id$";


#include <stdio.h>              /* for printf */
#include <stdlib.h>             /* for exit() */
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>               /* for time() */
#include <syslog.h>             /* for log levels */
#include <stddef.h>             /* for 'offsetof' */
#include <errno.h>
#include "/usr/local/include/rta.h"
#include "empd.h"



/***************************************************************************
 *  - Limits and defines
 *    Limits on the size and number of resources....
 ***************************************************************************/
/* Maximum number of file descriptors in select() loop */
#define MX_FD     (200)



/***************************************************************************
 *  - Data structures
 *    The structure to use to hold the timer info
 ***************************************************************************/
/***************************************************************
 * the information kept for each file descriptor
 **************************************************************/
typedef struct fd_desc
{
  int         fd;         // FD of TCP conn (=-1 if not in use)
  int         (*rcb)();   // Callback on select() read activity
  int         (*wcb)();   // Callback on select() write activity
  void       *pcb_data;   // data included in call of callbacks
  long long   nread;      // number of reads on FD
  long long   nwrite;     // number of writes on FD
  long long   nbrd;       // number of bytes read
  long long   nbwr;       // number of bytes written
  long long   nerr;       // number of error on FD
}
FD_DESC;



/***************************************************************************
 *  - Function prototypes
 ***************************************************************************/



/***************************************************************************
 *  - Variable allocation and initialization
 ***************************************************************************/
FD_DESC  Fd_Desc[MX_FD];


/***************************************************************
 * We make the above table if File Descriptors available using
 * RTA with the RTA columns and table defined below.
 **************************************************************/
COLDEF   Fd_DescCols[] = {
  {
      "Fd_Desc",                /* table name */
      "fd",                     /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(FD_DESC, fd),    /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "File descriptor for TCP connection or other IO"},
  {
      "Fd_Desc",                /* table name */
      "rcb",                    /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(FD_DESC, rcb),   /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
      "Address of the subroutine called when data is available "
      "for input"},
  {
      "Fd_Desc",                /* table name */
      "wcb",                    /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(FD_DESC, wcb),   /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
      "Address of the subroutine called when a FD waiting to write "
      "is unblocked."},
  {
      "Fd_Desc",                /* table name */
      "pcb_data",               /* column name */
      RTA_PTR,                  /* type of data */
      sizeof(void *),           /* #bytes in col data */
      offsetof(FD_DESC, pcb_data),    /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Data passed transparently into the read and write callbacks."},
  {
      "Fd_Desc",                /* table name */
      "nread",                   /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(long long),        /* #bytes in col data */
      offsetof(FD_DESC, nread),  /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Number of times we have done a read on this file descriptor."},
  {
      "Fd_Desc",                /* table name */
      "nwrite",                 /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(long long),        /* #bytes in col data */
      offsetof(FD_DESC, nwrite),  /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "times we have done a write on this file descriptor."},
  {
      "Fd_Desc",                /* table name */
      "nbrd",                   /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(long long),        /* #bytes in col data */
      offsetof(FD_DESC, nbrd),  /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Number of bytes received on this file descriptor"},
  {
      "Fd_Desc",                /* table name */
      "nbwr",                   /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(long long),        /* #bytes in col data */
      offsetof(FD_DESC, nbwr),  /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Number of bytes sent out on this file descriptor"},
  {
      "Fd_Desc",                /* table name */
      "nerr",                   /* column name */
      RTA_LONG,                 /* type of data */
      sizeof(long long),        /* #bytes in col data */
      offsetof(FD_DESC, nerr),  /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Number of errors reading or writing to this FD"},
};

/***************************************************************
 *   We defined all of the data structure (column definitions)
 * for the tables in our FD_DESC.  Now define the tables themselves.
 **************************************************************/
TBLDEF   Fd_DescTable = {
      "Fd_Desc",                /* table name */
      Fd_Desc,                  /* address of table */
      sizeof(FD_DESC),          /* length of each row */
      MX_FD,                    /* # rows in table */
      (void *) 0,               /* iterator function */
      (void *) 0,               /* iterator callback data */
      Fd_DescCols,              /* Column definitions */
      sizeof(Fd_DescCols) / sizeof(COLDEF), /* # columns */
      "",                       /* save file name */
      "Table of file descriptors being serviced by the select() loop."
};





/***************************************************************
 * selectInit(): - Initialize structures and variables before
 * starting the selecting loop.
 *
 * Input:        none
 * Output:       none
 * Effects:      manager connection table (ui)
 ***************************************************************/
void
selectInit()
{
  int i;

  for (i=0; i<MX_FD; i++) {
    Fd_Desc[i].fd = -1;
  }

  rta_add_table(&Fd_DescTable);
}




/***************************************************************
 * doSelect(): - enter the select() loop.  This is the main loop of
 * the program.  It manages all file IO and all timers.
 *
 * Input:        none
 * Output:       none
 * Effects:      Well, lots.  It *is* the main loop
 ***************************************************************/
void
doSelect()
{
  fd_set   rfds;       // read bit masks for select statement
  fd_set   wfds;       // write bit masks for select statement
  int      mxfd;       // Maximum FD for the select statement
  int      i;          // generic loop counter
  struct timeval *ptv; // points to value of next timeout
  int      ret;        // return value of select() call
  int      cnt;        // number of bytes read or written, -1 on err


  ptv = doTimer();

  while (1)
  {
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    mxfd = 0;

    for (i=0; i<MX_FD; i++) {
      if (Fd_Desc[i].fd < 0)
        continue;

      if (Fd_Desc[i].rcb) {
        FD_SET(Fd_Desc[i].fd, &rfds);
      }
      if (Fd_Desc[i].wcb) {
        FD_SET(Fd_Desc[i].fd, &wfds);
      }

      /* Set the maximum FD for the select call */
      mxfd = (Fd_Desc[i].fd > mxfd) ? Fd_Desc[i].fd : mxfd;
    }

    /* Wait for some something to do */
    ret = select(mxfd + 1, &rfds, &wfds, (fd_set *) 0, ptv);

    if (ret < 0) {     // select error  -- bail out
      LOG(LOG_ERR, SL, E_Bad_Select, errno);
      exit(-1);
    }

    for (i=0; i<MX_FD; i++) {
      if (Fd_Desc[i].fd < 0)
        continue;
      if (FD_ISSET(Fd_Desc[i].fd, &rfds) && Fd_Desc[i].rcb) {
        Fd_Desc[i].nread++;
        cnt = Fd_Desc[i].rcb(Fd_Desc[i].fd, Fd_Desc[i].pcb_data);
        if (cnt >= 0)
          Fd_Desc[i].nbrd += cnt;
        else
          Fd_Desc[i].nerr++;
      }

      if ((Fd_Desc[i].fd >= 0) &&
           FD_ISSET(Fd_Desc[i].fd, &wfds) &&
           Fd_Desc[i].wcb) {
        Fd_Desc[i].nwrite++;
        cnt = Fd_Desc[i].wcb(Fd_Desc[i].fd, Fd_Desc[i].pcb_data);
        if (cnt >= 0)
          Fd_Desc[i].nbwr += cnt;
        else
          Fd_Desc[i].nerr++;
      }
    }

    /* process timers and get next timeout value */
    ptv = doTimer();
  }
}



/***************************************************************
 * add_fd(): - add a file descriptor to the select list
 *
 * Input:        int   fd -- the file descriptor to add
 *               int   (*callback)() -- pointer to subroutine
 *                     called when select() detects activity
 *               int   cbdata -- callback data which is passed
 *                     to the callback
 * Output:       0 if fd is added OK,  -1 on any error
 * Effects:      No side effects
 ***************************************************************/
int add_fd(int fd,             // FD to add 
           int   (*rcb)(),     // read callback
           int   (*wcb)(),     // write callback
           void *pcb_data)     // callback data
{
  int i;            // loop counter

  /* Consistency check: fd must be positive and either the
   * read callback or the write callback must be defined.  */
  if ((rcb == NULL) && (wcb == NULL)) {
    return(remove_fd(fd));
  }
  if (fd < 0) {
    LOG(LOG_WARNING, SL, E_Bad_FD);
    return(-1);
  }

  /* Scan array of fd descriptors to see if fd is already in table */
  for (i=0; i<MX_FD; i++) {
    if (Fd_Desc[i].fd == fd) {
      Fd_Desc[i].rcb      = rcb;
      Fd_Desc[i].wcb      = wcb;
      Fd_Desc[i].pcb_data = pcb_data;
      return(0);
    }
  }

  /* Find an empty slot and use it for new fd */
  for (i=0; i<MX_FD; i++) {
    if (Fd_Desc[i].fd == -1) {
      Fd_Desc[i].fd       = fd;
      Fd_Desc[i].rcb      = rcb;
      Fd_Desc[i].wcb      = wcb;
      Fd_Desc[i].pcb_data = pcb_data;
      Fd_Desc[i].nread    = 0;
      Fd_Desc[i].nwrite   = 0;
      Fd_Desc[i].nbrd     = 0;
      Fd_Desc[i].nbwr     = 0;
      Fd_Desc[i].nerr     = 0;
      return(0);
    }
  }

  /* report error if no empty slots */
  if (i == MX_FD) {
    LOG(LOG_WARNING, SL, E_Mx_FD);
    return(-1);
  }
  return(0);
}



/***************************************************************
 * remove_fd(): - remove a file descriptor from the select list
 *
 * Input:        int   fd -- the file descriptor to remove
 * Output:       0 if fd is added OK,  -1 on any error
 * Effects:      No side effects
 ***************************************************************/
int remove_fd(int fd)
{
  int i;            // loop counter

  /* Locate fd and set it to -1 */
  for (i=0; i<MX_FD; i++) {
    if (Fd_Desc[i].fd == fd) {
      Fd_Desc[i].fd = -1;
      return(0);
    }
  }

  /* If we get here we could not find the fd to remove. */
  LOG(LOG_ERR, SL, E_No_FD, fd);

  return(-1);
}



