/***************************************************************************
 *                        empd.h
 *  copyright            : (C) 2004 by Bob Smith
 *
 *   This file contains the defines, variables, and procedure definitions
 * needed by all components in the empty daemon.
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


static char const Version_empd_h[] = "$Id$";



/***************************************************************************
 *  - Limits and defines
 *    Limits on the size and number of resources....
 ***************************************************************************/

/* The default TCP port for RTA connection from the UI programs */
#define DEF_UI_SERVER_PORT  8888


/* Timer types for use in add_timer() */
#define ED_ONESHOT  0
#define ED_PERIODIC 1


/* Macro and defines for the logging.  */
#define LOG(lvl, sect, fmt, args...) if (Logit[sect].thres >= lvl) \
 logit(lvl, sect, "%s:%d: " fmt "\n", __FILE__, __LINE__, ## args)

/* Log section IDs.  We intentionally keep these short */
#define MN    0    /* main() */
#define SL    1    /* select loop and file IO */
#define TM    2    /* timers */
#define CF    3    /* configuration */

/* The maximum length of the 'section' name */
#define LOGIT_NAME_LEN  (12)

/* The maximum comment length */
#define LOGIT_COMMENT_LEN  (255)




/***************************************************************************
 *  - Data structures
 ***************************************************************************/
/* The log threshold structure must be globally visible for 
   the LOG macro above to work. */
typedef struct
{
  char     sect[LOGIT_NAME_LEN]; // the section name
  int      thres;      // log threshold.  0-6=normal  7-15=debug
  int      output;     // 0=none,1=stderr,2=syslog,3=both
  char     comment[LOGIT_COMMENT_LEN]; // description of section
}
LOGIT;




/***************************************************************************
 *  - Variable allocation and initialization
 ***************************************************************************/
extern LOGIT Logit[];   // table of log thresholds




/***************************************************************************
 *  - Function prototypes
 ***************************************************************************/
void     configInit(int argc, char *argv[]);
void     selectInit();
void     uiInit();
void     timerInit();
void     logitInit();
void     appInit();
void     doSelect();
struct timeval * doTimer();


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
           void *pcb_data);    // callback data



/***************************************************************
 * remove_fd(): - remove a file descriptor from the select list
 *
 * Input:        int   fd -- the file descriptor to remove
 * Output:       0 if fd is added OK,  -1 on any error
 * Effects:      No side effects
 ***************************************************************/
int remove_fd(int fd);



/***************************************************************
 * add_timer(): - register a subroutine to be executed after a
 * specified number of milliseconds.
 *
 * Input:        int   type -- ED_ONESHOT or ED_PERIODIC
 *               void  (*cb)() -- pointer to subroutine called
 *                     when the timer expires
 *               void *pcbdata -- callback data which is passed
 *                     transparently to the callback
 * Output:       void * to be used as a 'handle' in the cancel call.
 * Effects:      No side effects
 ***************************************************************/
void * add_timer(int type,         // oneshot or periodic
           int   ms,               // milliseconds to timeout
           void  (*cb)(),          // timeout callback
           void *pcb_data);        // callback data



/***************************************************************
 * remove_timer(): - remove a timer from the queue.  The single
 * parameter is the void pointer returned when the timer was
 * added.
 *
 * Input:        Pointer returned by add_timer()
 * Output:       Nothing
 * Effects:      No side effects
 ***************************************************************/
void remove_timer(void *ptimer);



/***************************************************************
 * logit():  - Log a message to stderr or syslog.   The message
 * is logged if the log level passed is numerically less than or
 * equal to the threshold in the LOGIT table row specified by the
 * section ID in the call.  The file name and line number are passed
 * as part of the call too.
 *   The last parameters to the call are a format string and a va_arg
 * list of variables to print.
 *   This routine is not normally called directly.  Use the LOG
 * macro defined above.
 *   
 * Return: nothing.
 **************************************************************/
void logit(
  int level,           // Logged if threshold is higher or equal
  int sectID,          // Index into the LOGIT table
  char *format,        // Log message printf string
  ...);                // Parameters for the printf string


/**************************************************************
 * void logitSetEntry() - assign an entry in the Logit table.
 * You can edit the Logit table directly or use the call to set
 * a new logging section.  
 *   See the description of how logging works at the top of the
 * logit.c file.
 *
 * Output:    none
 * Affects:   none
 **************************************************************/
void logitSetEntry(
  int row,                 // row to set the values for
  char *sect,              // the section name
  int thres,               // the logging threshold
  int output,              // output  : 0=none, 1=stderr, 2=syslog, 3=both
  char *comment);          // a long description




/***************************************************************************
 *  - Log messages
 ***************************************************************************/

/*  The following is a list of the log messages that indicate a lack of
    resources in the daemon.  We try to continue on when these problems
    occur but you should investigate if you see any of the following: */
#define E_Mx_Conn     "Too many UI connections.  Dropping oldest one."
#define E_Mx_FD       "Too many file descriptors in select loop"
#define E_Mx_Timer    "Too many timers in use"

/*  The following errors indicate a problem with the environment in
    which the deamon is trying to run.  You should check the permissions
    and configuration to resolve these problems: */
#define E_Not_Alone   "Another daemon is already running. PID=%d"
#define E_Rta_Port    "Can not set up TCP socket for RTA port %d"
#define E_Conf_Dir    "Invalid configuration directory: %s"
#define E_Pid_File    "Unable to create new PID file.  File=%s"

/*  Some errors are generated if a sanity check fails.  You should
    check your code carefully if you see any of the following: */
#define E_Bad_Conn    "Can not accept TCP connection from the UI"
#define E_Bad_FD      "Can not listen on negative file descriptor"
#define E_No_FD       "Can not remove non-existent file descriptor, %d"
#define E_No_Wr       "Error #%d writing to TCP conn at port #%d on IP=%d"
#define E_No_Conn     "No connection for file descriptor = %d"
#define E_Bad_Tim     "Periodic timers must have non-zero intervals"
#define E_No_Timer    "Trying to unlink non-existent timer"

/*  And some errors should just not occur: */
#define E_Bad_Select   "select() error #%d -- exit"
#define E_No_Mem      "Unable to allocate memory"
#define E_No_Date     "Unable to get system time"



