/***************************************************************
 * appmain.c --
 *
 * Overview:
 *     This is a sample application which illustrates the use of
 * the rta package.  It uses select() for I/O multiplexing and
 * offers the tables of UI connections (uiconns) to the user
 * interface as a DB table (UIConns).
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
#include <time.h>             /* for time() function */
#include <errno.h>
#include "app.h"

#define  DB_PORT    8888

void accept_ui_session(int srvfd);
void compute_cdur(char *tbl, char *col, char *sql, int rowid);
void handle_ui_output(int indx);
void handle_ui_request(int indx);
void init_ui();
int listen_on_port(int port);
void reverse_str(char *tbl, char *col, char *sql, int rowid);
extern TBLDEF UITables[];
extern int    nuitables;

/*  -allocate static, global storage for tables and variables */
UI    ui[MX_UI];
struct MyData mydata[ROW_COUNT];


/***************************************************************
 * How this program works:
 *  - Allocate and initialize system variables (as DB tables)
 *  - Open socket to listen for DB config commands
 *  - select() loop
 **************************************************************/
int
main()
{
    fd_set rfds;       /* read bit masks for select statement */
    fd_set wfds;       /* write bit masks for select statement */
    int   mxfd;        /* Maximum FD for the select statement */
    int   newui_fd = -1; /* FD to TCP socket accept UI conns */
    int   i;           /* generic loop counter */

    /* Init the DB interface */
    rta_init ();
    for (i=0; i <nuitables; i++)
    {
        rta_add_table(&UITables[i]);
    }
    init_ui();

    while (1)
    {
        /* Build the fd_set for the select call.  This includes
           the listen port for new UI connections and any existing
           UI connections.  We also look for the ability to write
           to the clients if data is queued.  */
        FD_ZERO (&rfds);
        FD_ZERO (&wfds);
        mxfd = 0;

        /* open UI/DB/manager listener if needed */
        if (newui_fd < 0)
        {
            newui_fd = listen_on_port(DB_PORT);
        }
        FD_SET (newui_fd, &rfds);
        mxfd = (newui_fd > mxfd) ? newui_fd : mxfd;

        /* for each UI conn .... */
        for (i = 0; i < MX_UI; i++)
        {
            if (ui[i].fd < 0)           /* Conn open? */
                continue;
            if (ui[i].rspfree < MXRSP)  /* Data to send? */
            {
                FD_SET (ui[i].fd, &wfds);
                mxfd = (ui[i].fd > mxfd) ? ui[i].fd : mxfd;
            }
            else
            {
                FD_SET (ui[i].fd, &rfds);
                mxfd = (ui[i].fd > mxfd) ? ui[i].fd : mxfd;
            }
        }


        /* Wait for some something to do */
        (void) select (mxfd + 1, &rfds, &wfds,
                       (fd_set *) 0, (struct timeval *) 0);

        /* ....after the select call.  We have activity. 
           Search through the open fd's to find what to do. */

        /* Handle new UI/DB/manager connection requests */
        if ((newui_fd >= 0) && (FD_ISSET (newui_fd, &rfds)))
        {
            accept_ui_session (newui_fd);
        }

        /* process request from or data to one of the UI programs */
        for (i = 0; i < MX_UI; i++)
        {
            if (ui[i].fd < 0)
            {
                continue;
            }
            if (FD_ISSET(ui[i].fd, &rfds)) {
                handle_ui_request (i);
            }
            else if (FD_ISSET(ui[i].fd, &wfds)) {
                handle_ui_output(i);
            }
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
accept_ui_session (int srvfd)
{
    int   i;           /* generic loop counter */
    int   adrlen;      /* length of an inet socket address */
    struct sockaddr_in cliskt; /* socket to the UI/DB client */
    int   flags;       /* helps set non-blocking IO */
    int   oldtime, oldindx; /* helps find oldest conn */

    /* We have a new UI/DB/manager connection request.  So find
       a free slot and allocate it */
    for (i = 0; i < MX_UI; i++)
    {
        if (ui[i].fd < 0)
            break;
    }

    /* Probably a programming error if we don't have enough
       sessions.  For now, we force a drop on the oldest ui
       connection. */
    if (i == MX_UI)
    {
        syslog (LOG_ERR, "no manager connections");

        /* Use a linear search to find the oldest conn */
        oldtime = ui[0].ctm;
        oldindx = 0;
        for (i = 1; i < MX_UI; i++)
        {
            if (ui[i].ctm < oldtime)
            {
                oldtime = ui[i].ctm;
                oldindx = i;
            }
        }
        i = oldindx;  /* next part expect free ui indx in 'i' */
        close(ui[i].fd);
        ui[i].fd = -1;  
    }
 
    /* OK, we've got the ui slot, now accept the conn */
    adrlen = sizeof (struct sockaddr_in);
    ui[i].fd = accept (srvfd, (struct sockaddr *) &cliskt,
                       &adrlen);

    if (ui[i].fd < 0)
    {
        fprintf (stderr, "Manager accept() error (%d). \n",
                 errno);
        ui[i].fd = -1;
    }
    else
    {
        /* inc number ui, then init new ui */
        flags = fcntl (ui[i].fd, F_GETFL, 0);
        flags |= O_NONBLOCK;
        (void) fcntl (ui[i].fd, F_SETFL, flags);
        ui[i].o_ip    = (int) cliskt.sin_addr.s_addr;
        ui[i].o_port  = (int) ntohs(cliskt.sin_port);
        ui[i].cmdindx = 0;
        ui[i].rspfree = MXRSP;
        ui[i].ctm     = (int) time((time_t *) 0);
        ui[i].nbytin  = 0;
        ui[i].nbytout = 0;
    }
}

/***************************************************************
 * compute_cdur() - a read callback to compute the number of
 * seconds the TCP connection has been up.
 *
 * Input:        char *tbl   -- the table read (UIConns)
 *               char *col   -- the column read (cdur)
 *               char *sql   -- actual SQL of the command
 *               int  rowid  -- row number of row read 
 * Output:       none
 * Effects:      Computes the difference between the current
 *               value of time() and the value stored in the
 *               field 'ctm'.  The result is placed in 'cdur'.
 ***************************************************************/
void
compute_cdur(char *tbl, char *col, char *sql, int rowid)
{
    ui[rowid].cdur = ((int) time((time_t *) 0)) - ui[rowid].ctm;
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
 * Input:        index of the relevant entry in the ui table
 * Output:       none
 * Effects:      many, many side effects via table callbacks
 ***************************************************************/
void
handle_ui_request (int indx)
{
    int   ret;         /* a return value */
    int   dbstat;      /* a return value */
    int   t;           /* a temp int */

    /* We read data from the connection into the buffer in the
       ui struct.  Once we've read all of the data we can, we
       call the DB routine to parse out the SQL command and to
       execute it. */
    ret = read (ui[indx].fd,
                &(ui[indx].cmd[ui[indx].cmdindx]),
                (MXCMD - ui[indx].cmdindx));


    /* shutdown manager conn on error or on zero bytes read */
    if (ret <= 0)
    {
        /* log this since a normal close is with an 'X' command
           from the client program? */
        close (ui[indx].fd);
        ui[indx].fd = -1;
        return;
    }
    ui[indx].cmdindx += ret;
    ui[indx].nbytin += ret;

    /* The commands are in the buffer. Call the DB to parse and
       execute them */
    do
    {
        t = ui[indx].cmdindx,   /* packet in length */
            dbstat = dbcommand (ui[indx].cmd, /* packet in */
                                &ui[indx].cmdindx, /* packet in
                                                      length */
                                &ui[indx].rsp[MXRSP -
                                              ui[indx].rspfree],
                                &ui[indx].rspfree);
        t -= ui[indx].cmdindx,  /* t = # bytes consumed */

        /* move any trailing SQL cmd text up in the buffer */
        (void) memmove (ui[indx].cmd, &ui[indx].cmd[t], t);
    } while (dbstat == RTA_SUCCESS);

    /* the command is done (including side effects).  Send any
       reply back to the UI */
    handle_ui_output(indx);
}


/***************************************************************
 * handle_ui_output() - This routine is called to write data
 * to the TCP connection to the UI programs.  It is useful for
 * slow clients which can not accept the output in one big gulp.
 *
 * Input:        index of the relevant entry in the ui table
 * Output:       none
 * Effects:      none
 ***************************************************************/
void
handle_ui_output(int indx)
{
    int    ret;     /* write() return value */

    if (ui[indx].rspfree < MXRSP)
    {
        ret = write (ui[indx].fd, ui[indx].rsp, 
                     (MXRSP - ui[indx].rspfree));
        if (ret < 0)
        {
            /* log a failure to talk to a DB/UI connection */
            fprintf (stderr,
                     "error #%d on ui write to port #%d on IP=%d\n",
                     errno, ui[indx].o_port, ui[indx].o_ip);
            close (ui[indx].fd);
            ui[indx].fd = -1;
            return;
        }
        else if (ret == (MXRSP - ui[indx].rspfree))
        {
            ui[indx].rspfree = MXRSP;
            ui[indx].nbytout += ret;
        }
        else
        {
            /* we had a partial write.  Adjust the buffer */
            (void) memmove (ui[indx].rsp, &ui[indx].rsp[ret], 
                            (MXRSP - ui[indx].rspfree - ret));
            ui[indx].rspfree += ret;
            ui[indx].nbytout += ret;   /* # bytes sent on conn */
        }
    }
}


/***************************************************************
 * init_ui(): - Initializes the data structures for the DB
 * connections from the UIs.
 *
 * Input:        none
 * Output:       none
 * Effects:      manager connection table (ui)
 ***************************************************************/
void
init_ui ()
{
    int   i;           /* generic loop counter */

    for (i = 0; i < MX_UI; i++)
    {
        ui[i].fd = -1;
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
listen_on_port (int port)
{
    int   srvfd;       /* FD for our listen server socket */
    struct sockaddr_in srvskt;
    int   adrlen;
    int   flags;

    adrlen = sizeof (struct sockaddr_in);
    (void) memset ((void *) &srvskt, 0, (size_t) adrlen);
    srvskt.sin_family = AF_INET;
    srvskt.sin_addr.s_addr = INADDR_ANY;
    srvskt.sin_port = htons (port);
    if ((srvfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf (stderr, "Unable to get socket for port %d.",
                 port);
        exit(1);
    }
    flags = fcntl (srvfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    (void) fcntl (srvfd, F_SETFL, flags);
    if (bind (srvfd, (struct sockaddr *) &srvskt, adrlen) < 0)
    {
        fprintf (stderr, "Unable to bind to port %d\n", port);
        exit(1);
    }
    if (listen (srvfd, 1) < 0)
    {
        fprintf (stderr, "Unable to listen on port %d\n", port);
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
 *               int  rowid  -- row number of row modified
 * Output:       none
 * Effects:      Puts the reverse of 'notes' into 'seton'
 ***************************************************************/
void
reverse_str(char *tbl, char *col, char *sql, int rowid)
{
    int   i,j;                 /* loop counters */

    i = strlen(mydata[rowid].notes) -1;  /* -1 to ignore NULL */
    for(j=0 ; i>=0; i--,j++) {
        if (mydata[rowid].notes[i] == '<' ||
            mydata[rowid].notes[i] == '>')
            mydata[rowid].notes[i] = '.';
        mydata[rowid].seton[j] = mydata[rowid].notes[i];
    }
    mydata[rowid].seton[j] = (char) 0;
}

