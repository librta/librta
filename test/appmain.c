/******************************************************************
 * librta library
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the MIT License.
 *  See the file COPYING file.
 *****************************************************************/

/***************************************************************
 * appmain.c --
 *
 * Overview:
 *     This is a sample application which illustrates the use of
 * the librta package.
 **************************************************************/

/* Layout of this file:
 *  -includes, defines, and forward references
 *  -allocate storage for tables and variables
 *  -main() routine
 *  -other routines in alphabetical order
 *  -includes, defines, and forward references */

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>               /* for time() function */
#include <errno.h>
#include <ctype.h>
#include "app.h"

#define  DB_PORT    8888

void     accept_ui_session(int srvfd);
int      compute_cdur(char *tbl, char *col, char *sql, void *pr, int rowid);
void     handle_ui_output(UI *pui);
void     handle_ui_request(UI *pui);
void     init_ui();
int      listen_on_port(int port);
int      reverse_str(char *tbl, char *col, char *sql, void *pr, int rowid,
                     void *por);
void    *get_next_conn(void *prow, void *it_data, int rowid);
void    *get_next_dlist(void *prow, void *it_data, int rowid);
int      add_demolist(char *tbl, char *sql, void *pr);
void     del_demolist(char *tbl, char *sql, void *pr);

extern RTA_TBLDEF UITables[];
extern int nuitables;

/*  -allocate static, global storage for tables and variables */
UI      *ConnHead;  /* head of linked list of UI conns */
int      nui = 0;   /* number of open UI connections */
struct MyData mydata[ROW_COUNT];
DEMOLIST *DemoHead; /* head of demo linked list */


/***************************************************************
 * How this program works:
 *  - Allocate and initialize system variables (as DB tables)
 *  - Open socket to listen for DB config commands
 *  - select() loop
 **************************************************************/
int
main()
{
  fd_set   rfds;       /* read bit masks for select statement */
  fd_set   wfds;       /* write bit masks for select statement */
  int      mxfd;       /* Maximum FD for the select statement */
  int      newui_fd = -1; /* FD to TCP socket accept UI conns */
  int      i;          /* generic loop counter */
  UI      *pui;        /* pointer to a UI struct */
  UI      *nextpui;    /* points to next UI in list */



  /* Init */
  ConnHead = (UI *) NULL;
  DemoHead =  (DEMOLIST *) NULL;

  for (i = 0; i < nuitables; i++)
  {
    rta_add_table(&UITables[i]);
  }

  while (1)
  {
    /* Build the fd_set for the select call.  This includes the listen
       port for new UI connections and any existing UI connections.  We 
       also look for the ability to write to the clients if data is
       queued.  */
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    mxfd = 0;

    /* open UI/DB/manager listener if needed */
    if (newui_fd < 0)
    {
      newui_fd = listen_on_port(DB_PORT);
    }
    FD_SET(newui_fd, &rfds);
    mxfd = (newui_fd > mxfd) ? newui_fd : mxfd;

    /* for each UI conn .... */
    pui = ConnHead;
    while (pui)
    {
      if (pui->rspfree < MXRSP) /* Data to send? */
      {
        FD_SET(pui->fd, &wfds);
        mxfd = (pui->fd > mxfd) ? pui->fd : mxfd;
      }
      else
      {
        FD_SET(pui->fd, &rfds);
        mxfd = (pui->fd > mxfd) ? pui->fd : mxfd;
      }
      pui = pui->nextconn;
    }

    /* Wait for some something to do */
    (void) select(mxfd + 1, &rfds, &wfds,
      (fd_set *) 0, (struct timeval *) 0);

    /* ....after the select call.  We have activity. Search through
       the open fd's to find what to do. */

    /* Handle new UI/DB/manager connection requests */
    if ((newui_fd >= 0) && (FD_ISSET(newui_fd, &rfds)))
    {
      accept_ui_session(newui_fd);
    }

    /* process request from or data to one of the UI programs */
    pui = ConnHead;
    while (pui)
    {
      /* Get next UI now since pui struct may be freed in handle_ui.. */
      nextpui = pui->nextconn;
      if (FD_ISSET(pui->fd, &rfds))
      {
        handle_ui_request(pui);
      }
      else if (FD_ISSET(pui->fd, &wfds))
      {
        handle_ui_output(pui);
      }
      pui = nextpui;
    }
  }
}

/***************************************************************
 * accept_ui_session(): - Accept a new UI/DB/manager session.
 * This routine is called when a user interface program such
 * as Apache (for the web interface), the SNMP manager, or one
 * of the console interface programs tries to connect to the
 * data base interface socket to do DB like get's and set's.
 * The connection is actually established by the PostgreSQL
 * library attached to the UI program by an XXXXXX call.
 *
 * Input:        The file descriptor of the DB server socket
 * Output:       none
 * Effects:      manager connection table (ui)
 ***************************************************************/
void
accept_ui_session(int srvfd)
{
  int      newuifd;    /* New UI FD */
  u_int    adrlen;     /* length of an inet socket address */
  struct sockaddr_in cliskt; /* socket to the UI/DB client */
  int      flags;      /* helps set non-blocking IO */
  UI      *pnew;       /* pointer to the new UI struct */
  UI      *pui;        /* pointer to a UI struct */


  /* Accept the connection */
  adrlen = sizeof(struct sockaddr_in);
  newuifd = accept(srvfd, (struct sockaddr *) &cliskt, &adrlen); 
  if (newuifd < 0) {
    syslog(LOG_ERR, "Manager accept() error");
    return;
  }

  /* We've accepted the connection.  Now get a UI structure.
     Are we at our limit?  If so, drop the oldest conn. */
  if (nui >= MX_UI)
  {
    syslog(LOG_WARNING, "no manager connections");

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
    syslog(LOG_ERR, "Unable to allocate memory");
    close(newuifd);
    return;
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
  pnew->ctm = (int) time((time_t *) 0);
  pnew->nbytin = 0;
  pnew->nbytout = 0;
}

/***************************************************************
 * compute_cdur() - a read callback to compute the number of
 * seconds the TCP connection has been up.
 *
 * Input:        char *tbl   -- the table read (UIConns)
 *               char *col   -- the column read (cdur)
 *               char *sql   -- actual SQL of the command
 *               void *pr    -- points to row affected
 *               int  rowid  -- row number of row read 
 * Output:       0 (success)
 * Effects:      Computes the difference between the current
 *               value of time() and the value stored in the
 *               field 'ctm'.  The result is placed in 'cdur'.
 ***************************************************************/
int
compute_cdur(char *tbl, char *col, char *sql, void *pr, int rowid)
{
  if(pr)
    ((UI *)pr)->cdur = ((int) time((time_t *) 0)) - ((UI *)pr)->ctm;

  return(0);
}

/***************************************************************
 * handle_ui_request(): - This routine is called to read data
 * from the TCP connection to the UI  programs such as the web
 * UI and consoles.  The protocol used is that of Postgres and
 * the data is an encoded SQL request to select or update a 
 * system variable.  Note that the use of callbacks on reading
 * or writing means a lot of the operation of the program
 * starts from this execution path.  The input is an index into
 * the ui table for the manager with data ready.
 *
 * Input:        pointer to UI struct with data to read
 * Output:       none
 * Effects:      many, many side effects via table callbacks
 ***************************************************************/
void
handle_ui_request(UI *pui)
{
  int      ret;        /* a return value */
  int      dbstat;     /* a return value */
  int      t;          /* a temp int */

  /* We read data from the connection into the buffer in the ui struct. 
     Once we've read all of the data we can, we call the DB routine to
     parse out the SQL command and to execute it. */
  ret = read(pui->fd, &(pui->cmd[pui->cmdindx]), (MXCMD - pui->cmdindx));

  /* shutdown manager conn on error or on zero bytes read */
  if (ret <= 0)
  {
    /* log this since a normal close is with an 'X' command from the
       client program? */
    close(pui->fd);
    /* Free the UI struct */
    if (pui->prevconn)
      (pui->prevconn)->nextconn = pui->nextconn;
    else
      ConnHead = pui->nextconn;
    if (pui->nextconn)
      (pui->nextconn)->prevconn = pui->prevconn;
    free(pui);
    nui--;
    return;
  }
  pui->cmdindx += ret;
  pui->nbytin += ret;

  /* The commands are in the buffer. Call the DB to parse and execute
     them */
  t = pui->cmdindx;                        /* packet in length */
  do
  {
    dbstat = rta_dbcommand(pui->cmd,         /* packet in */
      &(pui->cmdindx),                       /* packet in length */
      &(pui->rsp[MXRSP - pui->rspfree]),     /* ptr to out buf */
      &(pui->rspfree));                      /* N bytes at out */
    t -= pui->cmdindx;                       /* t = # bytes consumed */
    /* move any trailing SQL cmd text up in the buffer */
    (void) memmove(pui->cmd, &(pui->cmd[t]), t);
  } while (dbstat == RTA_SUCCESS);
  /* the command is done (including side effects).  Send any reply back 
     to the UI.  You may want to check for RTA_CLOSE here. */
  handle_ui_output(pui);
}

/***************************************************************
 * handle_ui_output() - This routine is called to write data
 * to the TCP connection to the UI programs.  It is useful for
 * slow clients which can not accept the output in one big gulp.
 *
 * Input:        pointer to UI structure ready for write
 * Output:       none
 * Effects:      none
 ***************************************************************/
void
handle_ui_output(UI *pui)
{
  int      ret;        /* write() return value */

  if (pui->rspfree < MXRSP)
  {
    ret = write(pui->fd, pui->rsp, (MXRSP - pui->rspfree));
    if (ret < 0)
    {
      /* log a failure to talk to a DB/UI connection */
      fprintf(stderr,
        "error #%d on ui write to port #%d on IP=%d\n",
        errno, pui->o_port, pui->o_ip);
      close(pui->fd);
      /* Free the UI struct */
      if (pui->prevconn)
        (pui->prevconn)->nextconn = pui->nextconn;
      else
        ConnHead = pui->nextconn;
      if (pui->nextconn)
        (pui->nextconn)->prevconn = pui->prevconn;
      free(pui);
      nui--;
      return;
    }
    else if (ret == (MXRSP - pui->rspfree))
    {
      pui->rspfree = MXRSP;
      pui->nbytout += ret;
    }
    else
    {
      /* we had a partial write.  Adjust the buffer */
      (void) memmove(pui->rsp, &(pui->rsp[ret]),
        (MXRSP - pui->rspfree - ret));
      pui->rspfree += ret;
      pui->nbytout += ret;  /* # bytes sent on conn */
    }
  }
}


/***************************************************************
 * listen_on_port(int port): -  Open a socket to listen for
 * incoming TCP connections on the port given.  Return the file
 * descriptor if OK, and -1 on any error.  The calling routine
 * can handle any error condition.
 *
 * Input:        The interger value of the port number to bind to
 * Output:       The file descriptor of the socket
 * Effects:      none
 ***************************************************************/
int
listen_on_port(int port)
{
  int      srvfd;      /* FD for our listen server socket */
  struct sockaddr_in srvskt;
  int      adrlen;
  int      flags;

  adrlen = sizeof(struct sockaddr_in);
  (void) memset((void *) &srvskt, 0, (size_t) adrlen);
  srvskt.sin_family = AF_INET;
  //  srvskt.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  srvskt.sin_addr.s_addr = INADDR_ANY;
  srvskt.sin_port = htons(port);
  if ((srvfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Unable to get socket for port %d.", port);
    exit(1);
  }
  flags = fcntl(srvfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  (void) fcntl(srvfd, F_SETFL, flags);
  if (bind(srvfd, (struct sockaddr *) &srvskt, adrlen) < 0)
  {
    fprintf(stderr, "Unable to bind to port %d\n", port);
    exit(1);
  }
  if (listen(srvfd, 1) < 0)
  {
    fprintf(stderr, "Unable to listen on port %d\n", port);
    exit(1);
  }
  return (srvfd);
}


/***************************************************************
 * reverse_str(): - a write callback to replace '<' and '>' with
 * '.', and to store the reversed string of notes into seton.
 *
 * Input:        char *tbl   -- the table modified
 *               char *col   -- the column modified
 *               char *sql   -- actual SQL of the command
 *               void *pr    -- points to row 
 *               void *por   -- points to copy of row before update
 *               int  rowid  -- row number of row modified
 * Output:       0 (success)
 * Effects:      Puts the reverse of 'notes' into 'seton'
 ***************************************************************/
int
reverse_str(char *tbl, char *col, char *sql, void *pr, int rowid, void *por)
{
  int      i, j;       /* loop counters */

  i = strlen(mydata[rowid].notes) - 1; /* -1 to ignore NULL */
  for (j = 0; i >= 0; i--, j++)
  {
    if (mydata[rowid].notes[i] == '<' || mydata[rowid].notes[i] == '>')
      mydata[rowid].notes[i] = '.';
    mydata[rowid].seton[j] = mydata[rowid].notes[i];
  }
  mydata[rowid].seton[j] = (char) 0;

  if (mydata[rowid].myint == 999)
    return(1);
  return(0);
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
 * get_next_dlist(): - an 'iterator' on the demo linked list
 *
 * Input:        void *prow  -- pointer to current row
 *               void *it_data -- callback data.  Unused.
 *               int   rowid -- the row number.  Unused.
 * Output:       pointer to next row.  NULL on last row
 * Effects:      No side effects
 ***************************************************************/
void *
get_next_dlist(void *prow, void *it_data, int rowid)
{
  if (prow == (void *) NULL)
    return((void *) DemoHead);

  return((void *) ((DEMOLIST *)prow)->dlnxt);
}

/***************************************************************
 * add_demolist(): - INSERT callback on the demolist table
 *
 * Input:        char *tbl -- the name of the table
 *               char *sql -- the text of the insert command
 *               void *pr -- pointer to the allocated row
 * Output:       the zero-indexed row number for the added row.
 *               (Or -1 on row insertion failure.)
 * Effects:      No side effects
 ***************************************************************/
int
add_demolist(char *tbl, char *sql, void *pr)
{
  DEMOLIST **pdemo;  // pointer to a row
  int        i;      // our row index (and return value)

  /* "5", we hate "5".  Give us a "5" and we'll reject the row! */
  if (((DEMOLIST *) pr)->dllong == 5)
    return(-1);

  /* walk the linked list and add this row to the end */
  i = 0;
  pdemo = &DemoHead;
  while (*pdemo) {
    pdemo = (DEMOLIST **) &((*pdemo)->dlnxt);
    i++;
  }
  *pdemo = (DEMOLIST *) pr;
  ((DEMOLIST *) pr)->dlnxt = (DEMOLIST *) 0;

  /* here we add application specific initialization of the
     new row.  Not much to do in this demo app. */
  ((DEMOLIST *) pr)->dlid  = i;
 
  return(i);
}



/***************************************************************
 * del_demolist(): - DELETE callback on the demolist table
 *
 * Input:        char *tbl -- the name of the table
 *               char *sql -- the text of the insert command
 *               void *pr -- pointer to row to delete
 * Output:       void
 * Effects:      Frees allocated memory for the row.
 ***************************************************************/
void
del_demolist(char *tbl, char *sql, void *pr)
{
  DEMOLIST **prevpdemo;  // pointer/pointer to a row
  DEMOLIST *demo;        // pointer to a row

  /* walk the linked list to find the row and copy its next
   * pointer into the previous row's next pointer. */
  prevpdemo = &DemoHead;
  demo      = DemoHead;
  while (demo) {
    if (demo == (DEMOLIST *) pr)
      break;
    prevpdemo = (DEMOLIST **) &(demo->dlnxt);
    demo = demo->dlnxt;
  }

  /* demo will be zero if we reached the bottom of the list
   * without finding the target row.  */
  if (!demo)
    return;  /* failed.  No row deleted */

  /* Link past the delete row. */
  *prevpdemo = ((DEMOLIST *)pr)->dlnxt;

  /* delete the row pointers and then the row itself */
  free(((DEMOLIST *)pr)->dlpstr);     // A pointer to a string of NOTE_LEN bytes
  free(((DEMOLIST *)pr)->dlpint);     // points to an interger
  free(((DEMOLIST *)pr)->dlplong);    // points to a long type
  free(((DEMOLIST *)pr)->dlpfloat);   // point to a float
  free(pr);                           // the row itself

  return;
}


