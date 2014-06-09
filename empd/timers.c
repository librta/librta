/***************************************************************************
 *                        timers.c
 *  copyright            : (C) 2004 by Bob Smith
 *
 *   This file has the routines for the timers in empty daemon
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


static char const Version_timers_c[] = "$Id$";

/***************************************************************************
 * Timers in Empty Daemon
 * 
 * #define ED_ONESHOT  0
 * #define ED_PERIODIC 1
 * 
 * void * add_timer(
 *       int type;             // one-shot or periodic
 *       int ms;               // milliseconds to timeout
 *       void *(*callback)();  // called at timeout
 *       void *cb_data)        // blindly passed to callback
 * 
 * The 'add_timer' routine registers a subroutine ('callback')
 * for execution a set number of milliseconds from the time
 * of registration.  The timeout will occur repeatedly if the
 * type is ED_PERIODIC and will occur once if ED_ONESHOT.
 * Add_timer returns a 'handle' to help identify the timer
 * in other calls.
 * 
 * The callback returns nothing.  The callback routine has 
 * two parameters, the handle of the timer (void *) and the
 * callback data registered with the timer (cb_data).
 * 
 * A timer can be canceled with a call to remove_timer()
 * which is passed a single parameter, the handle returned
 * from add_timer().
 *
 * The 'handle' is, of course, a pointer to the timer
 * structure defined below.   Timers can be scheduled at
 * most 2**31 milliseconds in the future on machines with
 * 32 bit ints.  This is about 24 days.
 **************************************************************************/


#include <stdio.h>              /* for printf() */
#include <sys/types.h>
#include <time.h>               /* for time() function */
#include <sys/time.h>           /* for timezone struct */
#include <syslog.h>             /* for log levels */
#include <stdlib.h>             /* for malloc() */
#include <stddef.h>             /* for 'offsetof' */
#include <errno.h>
#include "/usr/local/include/rta.h"
#include "empd.h"


/***************************************************************************
 *  - Limits and defines
 *    Limits on the size and number of resources....
 ***************************************************************************/
    /* Maximum number of timers available */
#define MX_TIMER  (200)


/***************************************************************************
 *  - Data structures
 *    The structure to use to hold the timer info
 ***************************************************************************/
typedef struct ed_timer
{
  int               type;      // one-shot or periodic
  long long         to;        // ms since Dec 29, 1970 to timeout
  int               ms;        // ms from now to timeout
  void              (*cb)();   // Callback on timeout
  void             *pcb_data;  // data included in call of callbacks
  long long         count;     // number of times we've expired
  struct ed_timer  *next;      // next timer in list or NULL
  struct ed_timer  *prev;      // previous timer in list or NULL
}
ED_TIMER;



/***************************************************************************
 *  - Function prototypes
 ***************************************************************************/
long long   tv2ms(struct timeval *);
void        link_timer(ED_TIMER *);
int         unlink_timer(ED_TIMER *);
void       *get_next_timer(void *, void *, int);



/***************************************************************************
 *  - Variable allocation and initialization
 ***************************************************************************/
ED_TIMER  *TimerHead = (ED_TIMER *) 0;
int        nTimers = 0;


COLDEF   TimerColumns[] = {
  {
      "Timer",                  // the table name
      "type",                   // the column name
      RTA_INT,                  // data type
      sizeof(int),              // number of bytes
      offsetof(ED_TIMER, type), // location in struct
      RTA_READONLY,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The type of the timer.  A zero indicates a timer that expires "
      "once and is not rescheduled, a ONESHOT timer.  A one indicates "
      "a timer that expires and is automatically rescheduled, a "
      "PERIODIC timer."
  },
  {
      "Timer",                  // the table name
      "to",                     // the column name
      RTA_LONG,                 // data type
      sizeof(long long),        // number of bytes
      offsetof(ED_TIMER, to),   // location in struct
      RTA_READONLY,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The number of milliseconds after the Epoch that this timer "
      "expires."
  },
  {
      "Timer",                  // the table name
      "ms",                     // the column name
      RTA_INT,                  // data type
      sizeof(int),              // number of bytes
      offsetof(ED_TIMER, ms),   // location in struct
      RTA_READONLY,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The number of milliseconds from when the timer was created "
      "to its expiration.  For a PERIODIC timer, this is the number "
      "milliseconds between calls to the timer callback."
  },
  {
      "Timer",                  // the table name
      "count",                  // the column name
      RTA_LONG,                 // data type
      sizeof(long long),        // number of bytes
      offsetof(ED_TIMER, count),// location in struct
      RTA_READONLY,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The number of timers this timer has expired.  Meaningful "
      "only for PERIODIC timers."
  },
  {
      "Timer",                  // the table name
      "cb",                     // the column name
      RTA_PTR,                  // data type
      sizeof(void *),           // number of bytes
      offsetof(ED_TIMER, cb),   // location in struct
      RTA_READONLY,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The address of the subroutine to be called when the timer "
      "expires."
  },
  {
      "Timer",                  // the table name
      "pcb_data",               // the column name
      RTA_PTR,                  // data type
      sizeof(void *),           // number of bytes
      offsetof(ED_TIMER, pcb_data), // location in struct
      RTA_READONLY,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The data to be passed as an input parameter to the callback "
      "executed when the timer expires.  The other parameter passed "
      "to the callback is a 'handle' returned when the timer was "
      "created."
  },
};

TBLDEF   TimerTable = {
  "Timer",                      // table name
  (void *) 0,                   // address of table
  sizeof(ED_TIMER),             // length of each row
  0,                            // number of rows
  get_next_timer,               // iterator callback
  (void *) 0,                   // iterator callback data
  TimerColumns,                 // array of column defs
  sizeof(TimerColumns) / sizeof(COLDEF), // the number of columns
  "",                           // save file
  "A linked list of the current timers in the system."
};




/***************************************************************
 * timerInit():  - Initialize all internal variables and tables
 * for the timer facility.
 * 
 * Input:  None
 * Return: None
 **************************************************************/
void     timerInit()
{
  rta_add_table(&TimerTable);
}




/***************************************************************
 * doTimers(): - Scan the timer queue looking for expired timers.
 * Call the callbacks for any expired timers and either remove
 * them (ED_ONESHOT) or reschedule them (ED_PERIODIC).
 *   Output a NULL timeval pointer if there are no timer or a
 * pointer to a valid timeval struct if there are timers.
 *
 * Input:        none
 * Output:       pointer to a timeval struct
 * Effects:      none
 ***************************************************************/
struct timeval *
doTimer()
{
  ED_TIMER   *pt;         // pointer to a timer struct
  ED_TIMER   *pt_next;    // pointer next timer in list
  struct timeval  tv;     // timeval struct to hold "now"
  struct timezone tz;     // timezone struct to how "now"
  long long       now;    // "now" in milliseconds since Epoch
  // the following is the allocation for the tv used in select()
  static struct timeval  select_tv;


  /* Just return if there are no timers defined */
  if (TimerHead == (ED_TIMER *) 0) {
    return((struct timeval *) 0);
  }

  /* Get "now" in milliseconds since the Epoch */
  if (gettimeofday(&tv, &tz)) {
    LOG(LOG_WARNING, TM, E_No_Date);
    return((struct timeval *) 0);
  }
  now = tv2ms(&tv);

  /* Walk the list looking for timers with a timeout less than now */
  pt = TimerHead;
  while ((pt) && (pt->to <= now)) {
    pt_next = pt->next;           // save since we'll alter next

    (pt->cb)(pt, pt->pcb_data);   // Do the callback
     pt->count++;                 // increment the count

    if (pt->type == ED_ONESHOT) { // if ONESHOT remove the timer
      remove_timer(pt);
    }
    else {                        // Periodic, so reschedule
      (void) unlink_timer(pt);
      pt->to += pt->ms;
      link_timer(pt);
    }

    /* look at next timer in list */
    pt = pt_next;
  }

  /* We processed all the expired timers.  Now we need to set the
     timeval struct to be used in the next select call.  This is
     null if there are no timers.  If there are timers then the
     select timeval is based on the next timer timeout value. */

  if (TimerHead == (ED_TIMER *) 0) {
    return((struct timeval *) 0);
  }

  select_tv.tv_sec  = (TimerHead->to - now) / 1000;
  select_tv.tv_usec = (suseconds_t) (((TimerHead->to - now) % 1000) * 1000);

  return(&select_tv);
}



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
           void *pcb_data)         // callback data
{
  ED_TIMER   *pnew;       // pointer to the new timer struct
  struct timeval  tv;     // timeval struct to hold "now"
  struct timezone tz;     // timezone struct to how "now"

  /* Are we at the allowed limit of timers? */
  if (nTimers >= MX_TIMER) {
    LOG(LOG_WARNING, TM, E_Mx_Timer);
    return((ED_TIMER *) 0);
  }

  /* Sanity check */
  if (ms == 0 && type == ED_PERIODIC) {
    LOG(LOG_WARNING, TM, E_Bad_Tim);
    return((ED_TIMER *) 0);
  }

  /* Alloc memory for new timer.  On error: log and continue */
  pnew = malloc(sizeof (ED_TIMER));
  if (pnew == (ED_TIMER *) NULL) {
    LOG(LOG_ERR, TM, E_No_Mem);
    return((ED_TIMER *) 0);
  }
  nTimers++;       /* increment number of ED_TIMER structs alloc'ed */

  /* Get "now" */
  if (gettimeofday(&tv, &tz)) {
    LOG(LOG_WARNING, TM, E_No_Date);
    return((ED_TIMER *) 0);
  }

  /* OK, we've got the ED_TIMER struct, now fill it in */
  pnew->type      = type;             // one-shot or periodic
  pnew->to        = tv2ms(&tv) + ms;  // ms from Epoch to timeout
  pnew->ms        = ms;               // ms from now to timeout
  pnew->cb        = cb;               // Callback on timeout
  pnew->pcb_data  = pcb_data;         // data included in call of callbacks
  pnew->count     = 0;                // Count is zero to start

  /* Insert timer in the queue */
  link_timer(pnew);

  return(pnew);
}



/***************************************************************
 * remove_timer(): - remove a timer from the queue
 *
 * Input:        Pointer to a timer structure
 * Output:       Nothing
 * Effects:      No side effects
 ***************************************************************/
void remove_timer(void *ptimer)
{
  if (!unlink_timer(ptimer)) {
    free(ptimer);
  }
}



/***************************************************************
 * tv2ms(): - convert a timeval struct to long long of milliseconds.
 *
 * Input:        Pointer to a timer structure
 * Output:       long long
 * Effects:      No side effects
 ***************************************************************/
long long tv2ms(struct timeval *ptv)
{
  return((ptv->tv_sec * 1000) + (ptv->tv_usec / 1000));
}



/***************************************************************
 * link_timer(): - insert a ED_TIMER into the linked list.
 *
 * Input:        Pointer to a ED_TIMER
 * Output:       void
 * Effects:      No side effects
 ***************************************************************/
void link_timer(ED_TIMER *ptimer)
{
  ED_TIMER   *pt;         // pointer to a timer struct
  int         insert_in_front = 0;   // infront or behind


  /* Insert the ED_TIMER in the list of timers */
  if (TimerHead == (ED_TIMER *) NULL) {
    TimerHead = ptimer;
    ptimer->prev = (ED_TIMER *) NULL;
    ptimer->next = (ED_TIMER *) NULL;
  }
  else {
    /* timers are in a sorted linked list with nearest timeout at the
       head of the linked list.  Search down the list to insert it. */
    pt = TimerHead;
    while (pt != (ED_TIMER *) NULL) {
      if (ptimer->to < pt->to) { 
        insert_in_front = 1;
        break;
      }
      else if (pt->next == (ED_TIMER *) 0) {
        insert_in_front = 0;
        break;
      }
      pt = pt->next;
    }

    /* Insert in_front or behind pt ? */
    if (insert_in_front) {
      ptimer->prev = pt->prev;
      pt->prev = ptimer;
      ptimer->next = pt;
      if (ptimer->prev) {
        (ptimer->prev)->next = ptimer;
      }
      else {
        TimerHead = ptimer;
      }
    }
    else {
      ptimer->next = (ED_TIMER *) 0;
      ptimer->prev = pt;
      pt->next = ptimer;
    }
  }
}



/***************************************************************
 * unlink_timer(): - unlink a ED_TIMER from the linked list.
 *
 * Input:        Pointer to a ED_TIMER
 * Output:       0 on success, -1 on error
 * Effects:      No side effects
 ***************************************************************/
int unlink_timer(ED_TIMER *ptimer)
{
  ED_TIMER   *pt;         // pointer to a timer struct


  LOG(7, TM, "Unlink timer at 0x%0lx", (long) ptimer);

  /* Walk the list looking for a ED_TIMER pointed to by ptimer */
  pt = TimerHead;
  while (pt != (ED_TIMER *) 0) {
      if (pt == (ED_TIMER *) ptimer)
        break;
      pt = pt->next;
  }

  /* At end of list or at ptimer.  Error if at end of list */
  if (pt == (ED_TIMER *) 0) {
    LOG(LOG_WARNING, TM, E_No_Timer);
    return(-1);
  }

  /* update links */
  if (pt == TimerHead) {
    TimerHead = pt->next;
  }
  if (pt->prev) {
    (pt->prev)->next = pt->next;
  }
  if (pt->next) {
    (pt->next)->prev = pt->prev;
  }

  return(0);
}



/***************************************************************
 * get_next_timer(): - an 'iterator' on the linked list of 
 * timers.
 *
 * Input:        void *prow  -- pointer to current row
 *               void *it_data -- callback data.  Unused.
 *               int   rowid -- the row number.  Unused.
 * Output:       pointer to next row.  NULL on last row
 * Effects:      No side effects
 ***************************************************************/
void *
get_next_timer(void *prow, void *it_data, int rowid)
{
  if (prow == (void *) 0)
    return((void *) TimerHead);

  return((void *) ((ED_TIMER *)prow)->next);
}


