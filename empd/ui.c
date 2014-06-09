/***************************************************************************
 *                        ui.c
 *  copyright            : (C) 2004 by Bob Smith
 *
 *   The routines and structures associated the RTA based user interface.
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


static char const Version_ui_c[] = "$Id$";


#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <syslog.h>              /* for log levels */
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>             /* for 'offsetof' */
#include <arpa/inet.h>          /* for inet_addr() */
#include "/usr/local/include/rta.h"
#include "empd.h"



/***************************************************************************
 *  - Limits and defines
 *    Limits on the size and number of resources....
 ***************************************************************************/
    /* Maximum number of UI/Postgres/RTA connections */
#define MX_UI     (20)

    /* Max size of a Postgres packet from/to the UI's */
#define MXCMD     (1000)
#define MXRSP     (50000)



/***************************************************************************
 *  - Function prototypes
 ***************************************************************************/
int     accept_ui_session(int srvfd, int cb_data);
void    compute_cdur(char *tbl, char *col, char *sql, void *pr, int rowid);
int     handle_ui_output(int, int);
int     handle_ui_request(int, int);
void    open_rta_port(int, char*);
void   *get_next_conn(void *prow, void *it_data, int rowid);


/***************************************************************************
 *  - Data structures
 *    The structure to use to hold the configuration values.
 ***************************************************************************/

/*  the information kept for TCP connections to UI programs */
typedef struct ui
{
  struct ui  *prevconn;   /* Points to previous conn in linked list */
  struct ui  *nextconn;   /* Points to next conn in linked list */
  int         fd;         /* FD of TCP conn (=-1 if not in use) */
  int         cmdindx;    /* Index of next location in cmd buffer */
  char        cmd[MXCMD]; /* SQL command from UI program */
  int         rspfree;    /* Number of free bytes in rsp buffer */
  char        rsp[MXRSP]; /* SQL response to the UI program */
  int         o_port;     /* Other-end TCP port number */
  int         o_ip;       /* Other-end IP address */
}
UI;



/***************************************************************************
 *  - Variable allocation and initialization
 ***************************************************************************/
UI      *ConnHead = (UI *) NULL;  // head of linked list of UI conns
int      nui = 0;                 // number of open UI connections


/***************************************************************
 * One of the tables we want to present to the UI is the table
 * of UI connections which is defined above.
 *
 * The following array of COLDEF describes this structure with
 * one COLDEF for each element in the UI structure.
 **************************************************************/
COLDEF   ConnCols[] = {
  {
      "UIConns",                /* table name */
      "fd",                     /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, fd),         /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "File descriptor for TCP socket to UI program"},
  {
      "UIConns",                /* table name */
      "cmdindx",                /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, cmdindx),    /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Index of the next free byte in the input string, cmd"},
  {
      "UIConns",                /* table name */
      "cmd",                    /* column name */
      RTA_STR,                  /* type of data */
      MXCMD,                    /* #bytes in col data */
      offsetof(UI, cmd),        /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Input command from the user interface program.  This"
      " is an SQL command which is executed against the data"
      " in the application."},
  {
      "UIConns",                /* table name */
      "rspfree",                /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, rspfree),    /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Index of the next free byte in the response string, rsp"},
  {
      "UIConns",                /* table name */
      "rsp",                    /* column name */
      RTA_STR,                  /* type of data */
      50,                       /* first 50 bytes of response field */
      offsetof(UI, rsp),        /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "Response back to the calling program.  This is used to"
      " store the result so that the write() does not need to"
      " block"},
  {
      "UIConns",                /* table name */
      "o_port",                 /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, o_port),     /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "The IP port of the other end of the connection"},
  {
      "UIConns",                /* table name */
      "o_ip",                   /* column name */
      RTA_INT,                  /* type of data */
      sizeof(int),              /* #bytes in col data */
      offsetof(UI, o_ip),       /* offset from col start */
      RTA_READONLY,             /* Flags for read-only/disksave */
      (void (*)()) 0,           /* called before read */
      (void (*)()) 0,           /* called after write */
    "The IP address of the other end of the connection"
      " cast to an int."},
};

/***************************************************************
 * Now define the RTA accessible table for the UI linked list
 **************************************************************/
TBLDEF   UIConns = {
      "UIConns",                /* table name */
      (void *) 0,               /* address of table */
      sizeof(UI),               /* length of each row */
      0,                        /* # rows in table */
      get_next_conn,            /* iterator function */
      (void *) NULL,            /* iterator callback data */
      ConnCols,                 /* Column definitions */
      sizeof(ConnCols) / sizeof(COLDEF), /* # columns */
      "",                       /* save file name */
    "Data about TCP connections from UI frontend programs"
};




/***************************************************************
 * accept_ui_session(): - Accept a new UI/DB/manager session.
 * This routine is called when a user interface program such
 * as Apache (for the web interface), the SNMP manager, or one
 * of the console interface programs tries to connect to the
 * data base interface socket to do DB like get's and set's.
 * This routine is the read callback for the UI listen socket.
 *
 * Input:        The file descriptor of the DB server socket
 *               and callback data
 * Output:       The number of bytes read or written (= 0)
 * Effects:      manager connection table (ui)
 ***************************************************************/
int
accept_ui_session(int srvfd, int cb_data)
{
  int      newuifd;    /* New UI FD */
  int      adrlen;     /* length of an inet socket address */
  struct sockaddr_in cliskt; /* socket to the UI/DB client */
  int      flags;      /* helps set non-blocking IO */
  UI      *pnew;       /* pointer to the new UI struct */
  UI      *pui;        /* pointer to a UI struct */

  /* Accept the connection */
  adrlen = sizeof(struct sockaddr_in);
  newuifd = accept(srvfd, (struct sockaddr *) &cliskt, &adrlen); 
  if (newuifd < 0) {
    LOG(LOG_ERR, CF, E_Bad_Conn);
    return(0);
  }

  /* We've accepted the connection.  Now get a UI structure.
     Are we at our limit?  If so, drop the oldest conn. */
  if (nui >= MX_UI)
  {
    LOG(LOG_WARNING, CF, E_Mx_Conn);

    /* oldest conn is one at head of linked list.  Close it and
       promote next oldest to the top of the linked list.  */
    close(ConnHead->fd);
    pui = ConnHead->nextconn;  
    free(ConnHead);
    nui--;
    ConnHead = pui;
    ConnHead->prevconn = (UI *) NULL;
  }

  pnew = malloc(sizeof (UI));
  if (pnew == (UI *) NULL) 
  {
    /* Unable to allocate memory for new connection.  Log it,
       then drop new connection.  Try to go on.... */
    LOG(LOG_ERR, CF, E_No_Mem);
    close(newuifd);
    return(0);
  }
  nui++;       /* increment number of UI structs alloc'ed */

  /* OK, we've got the UI struct, now add it to end of list */
  if (ConnHead == (UI *) NULL) 
  {
    ConnHead = pnew;
    pnew->prevconn = (UI *) NULL;
    pnew->nextconn = (UI *) NULL;
  }
  else
  {
    pui = ConnHead;
    while (pui->nextconn != (UI *) NULL)
      pui = pui->nextconn;
    pui->nextconn = pnew;
    pnew->prevconn = pui;
    pnew->nextconn = (UI *) NULL;
  }

  /* UI struct is now at end of list.  Fill it in.  */
  pnew->fd = newuifd;
  flags = fcntl(pnew->fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  (void) fcntl(pnew->fd, F_SETFL, flags);
  pnew->o_ip = (int) cliskt.sin_addr.s_addr;
  pnew->o_port = (int) ntohs(cliskt.sin_port);
  pnew->cmdindx = 0;
  pnew->rspfree = MXRSP;

  /* add the new UI conn to the read fd_set in the select loop */
  add_fd(newuifd, handle_ui_request, (int (*)()) NULL, (void *) 0);

  return(0);
}



/***************************************************************
 * handle_ui_request(): - This routine is called to read data
 * from the TCP connection to the UI  programs such as the web
 * UI and consoles.  The protocol used is that of Postgres and
 * the data is an encoded SQL request to select or update a 
 * system variable.  The use of RTA table callbacks on reading
 * or writing means a lot of the operation of the program
 * starts from this execution path.  The input is the FD for
 * the manager with data ready.
 *
 * Input:        FD of socket with data to read
 * Output:       The number of bytes read
 * Effects:      many, many side effects via table callbacks
 ***************************************************************/
int
handle_ui_request(int fd_in, int cb_data)
{
  UI      *pui;        // pointer to the active UI struct 
  int      nrd;        // number of bytes read
  int      dbstat;     // a return value
  int      t;          // a temp int

  /* Locate the UI struct with fd equal to fd_in */
  pui = ConnHead;
  while (pui) {
    if (pui->fd == fd_in)
      break;

    pui = pui->nextconn;
  }
  /* Error if we could not find the fd */
  if (!pui) {
    LOG(LOG_ERR, CF, E_No_Conn, fd_in);

    /* Take this bogus fd out of the select loop */
    remove_fd(fd_in);
    return(0);
  }

  /* We read data from the connection into the buffer in the ui struct. 
     Once we've read all of the data we can, we call the DB routine to
     parse out the SQL command and to execute it. */
  nrd = read(pui->fd, &(pui->cmd[pui->cmdindx]), (MXCMD - pui->cmdindx));

  /* shutdown manager conn on error or on zero bytes read */
  if (nrd <= 0)
  {
    /* log this since a normal close is with an 'X' command from the
       client program? */
    close(pui->fd);
    remove_fd(pui->fd);
    /* Free the UI struct */
    if (pui->prevconn)
      (pui->prevconn)->nextconn = pui->nextconn;
    else
      ConnHead = pui->nextconn;
    if (pui->nextconn)
      (pui->nextconn)->prevconn = pui->prevconn;
    free(pui);
    nui--;
    return(0);
  }
  pui->cmdindx += nrd;

  /* The commands are in the buffer. Call the DB to parse and execute
     them */
  do
  {
    t = pui->cmdindx;                        /* packet in length */
      dbstat = dbcommand(pui->cmd,           /* packet in */
      &(pui->cmdindx),                       /* packet in length */
      &(pui->rsp[MXRSP - pui->rspfree]),     /* ptr to out buf */
      &(pui->rspfree));                      /* N bytes at out */
    t -= pui->cmdindx;      /* t = # bytes consumed */
    /* move any trailing SQL cmd text up in the buffer */
    (void) memmove(pui->cmd, &(pui->cmd[t]), t);
  } while (dbstat == RTA_SUCCESS);
  /* the command is done (including side effects).  Send any reply back 
     to the UI.   You may want to check for RTA_CLOSE here. */

  /* Since we have something to output, we add a write callback to
     to the select loop for this connection. */
  add_fd(pui->fd, handle_ui_request, handle_ui_output, (void *) 0);

  /* lastly, return the number of bytes read */
  return(nrd);
}



/***************************************************************
 * handle_ui_output() - This routine is called to write data
 * to the TCP connection to the UI programs.  It is useful for
 * slow clients which can not accept the output in one big gulp.
 *
 * Input:        int fd_in: the file descriptor ready to write
 *               int cb_data: (ignored)
 * Output:       none
 * Effects:      none
 ***************************************************************/
int
handle_ui_output(int fd_in, int cb_data)
{
  int      nwt;        /* write() return value */
  UI      *pui;

  /* Locate the UI struct with fd equal to fd_in */
  pui = ConnHead;
  while (pui) {
    if (pui->fd == fd_in)
      break;

    pui = pui->nextconn;
  }
  /* Error if we could not find the fd */
  if (!pui) {
    LOG(LOG_ERR, CF, E_No_Conn, fd_in);

    /* Take this bogus fd out of the select loop */
    remove_fd(fd_in);
    return(0);
  }


  if (pui->rspfree < MXRSP)
  {
    nwt = write(pui->fd, pui->rsp, (MXRSP - pui->rspfree));
    if (nwt < 0)
    {
      /* log a failure to talk to a DB/UI connection */
      LOG(LOG_ERR, CF, E_No_Wr, errno, pui->o_port, pui->o_ip);
      close(pui->fd);
      remove_fd(pui->fd);
      /* Free the UI struct */
      if (pui->prevconn)
        (pui->prevconn)->nextconn = pui->nextconn;
      else
        ConnHead = pui->nextconn;
      if (pui->nextconn)
        (pui->nextconn)->prevconn = pui->prevconn;
      free(pui);
      nui--;
      return(0);
    }
    else if (nwt == (MXRSP - pui->rspfree))
    {
      /* Full buffer is available and no need for a write callback */
      pui->rspfree = MXRSP;
      add_fd(pui->fd, handle_ui_request, (int (*)()) NULL, (void *) 0);
    }
    else
    {
      /* we had a partial write.  Adjust the buffer */
      (void) memmove(pui->rsp, &(pui->rsp[nwt]),
        (MXRSP - pui->rspfree - nwt));
      pui->rspfree += nwt;

      /* add fd to select loop's fd_set for writes and reads */
      add_fd(pui->fd, handle_ui_request, handle_ui_output, (void *) 0);
    }
  }
  return(nwt);
}



/***************************************************************
 * open_rta_port(): - Open the RTA port for this application.
 * If a socket is already open, close it before trying the
 * new port.
 *   This routine can be called after init by a callback on the
 * configuration table if the user changes the RTA listen port.
 * Changing the RTA port should not be done lightly since it 
 * will almost certainly confuse all the user interface programs.
 *
 * Input:        int rta_port
 *               char *rta_addr;  the IP address to bind to
 * Output:       none
 * Effects:      select listen table
 ***************************************************************/
void
open_rta_port(int rta_port, char *rta_addr)
{
  static int srvfd = -1;   /* FD for RTA socket */
  struct sockaddr_in srvskt;
  int      adrlen;
  int      flags;

  /* If a re-init, close the current socket */
  if (srvfd >= 0) {
    remove_fd(srvfd);
    (void) close(srvfd);
  }

  adrlen = sizeof(struct sockaddr_in);
  (void) memset((void *) &srvskt, 0, (size_t) adrlen);
  srvskt.sin_family = AF_INET;
  if (rta_addr[0]) {
    srvskt.sin_addr.s_addr = inet_addr(rta_addr);
  }
  else {
    srvskt.sin_addr.s_addr = INADDR_ANY;
  }
  srvskt.sin_port = htons(rta_port);
  if ((srvfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    LOG(LOG_ERR, CF, E_Rta_Port, rta_port);
    return;
  }
  flags = fcntl(srvfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  (void) fcntl(srvfd, F_SETFL, flags);
  if (bind(srvfd, (struct sockaddr *) &srvskt, adrlen) < 0)
  {
    LOG(LOG_ERR, CF, E_Rta_Port, rta_port);
    return;
  }
  if (listen(srvfd, 5) < 0)
  {
    LOG(LOG_ERR, CF, E_Rta_Port, rta_port);
    return;
  }

  /* If we get to here, then we were able to open the RTA socket.
     Tell the select loop about it. */
  add_fd(srvfd, accept_ui_session, (int (*)()) NULL, (void *) 0);
}



/***************************************************************
 * get_next_conn(): - an 'iterator' on the linked list of TCP
 * connections.
 *
 * Input:        void *prow  -- pointer to current row
 *               void *it_data -- callback data.  Unused.
 *               int   rowid -- the row number.  Unused.
 * Output:       pointer to next row.  NULL on last row
 * Effects:      No side effects
 ***************************************************************/
void *
get_next_conn(void *prow, void *it_data, int rowid)
{
  if (prow == (void *) NULL)
    return((void *) ConnHead);

  return((void *) ((UI *)prow)->nextconn);
}


/***************************************************************
 * uiInit():  - Initialize all internal variables and tables.
 * 
 * Input:  None
 * Return: None
 **************************************************************/
void     uiInit()
{
  rta_add_table(&UIConns);
}


