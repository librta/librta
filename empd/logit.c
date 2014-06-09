/***************************************************************************
 *                        logit.c
 *  copyright            : (C) 2004 by Bob Smith
 *
 *   This file contains the logging and debug code for an empty daemon.
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


static char const Version_logit_c[] = "$Id$";

#include <stdio.h>
#include <sys/syslog.h>
#include <stddef.h>             /* for 'offsetof' */
#include <stdarg.h>             /* for va_arg */
#include <string.h>             /* for strncpy() */
#include "/usr/local/include/rta.h"
#include "empd.h"


/***************************************************************************
 * Overview:
 *   We divide the application into "sections" and can set the debug level
 * differently for each section.  For each section we have a section name,
 * debug level, output destination, and description.  This information is
 * in a table and the row number in the table is the numeric ID for that
 * section.   Output destinations include stderr ('E') and syslog ('S').
 * 
 *   Consider the following example:
 *
 *   Section  |  Level  |  Output  |  Comment
 *   ---------|---------|----------|---------
 *   poll     |  7      |  E       |  Code in poll()/select() loop
 *   rta      |  4      |  SE      |  DB interface code
 *   init     |  4      |  SE      |  Initialization code section
 *
 * For this table we would ....
 *  #define DBG_POLL  0
 *  #define DBG_RTA   1
 *  #define DBG_INIT  2
 *
 *   The above table would send WARNING and above for rta and init to both
 * stderr and to syslog.  The poll section is in a debug/trace mode with
 * output going only to stderr.
 *
 *  To continue the above example, this would send output to stderr....
 *    logMsg(LOG_INFO, DBG_POLL, __FILE__, __NAME__, "got poll() debug event");
 *
 *  But this would generate no output....
 *    logMsg(LOG_INFO, DBG_INIT, __FILE__, __NAME__, "got init INFO event");
 *
 *   Your choice as developer is to decide what gets logged, what section
 * a log message belongs to, and what threshold triggers output of your
 * log message.
 *
 ***************************************************************************/


/***************************************************************************
 *  - Limits and defines
 *    Limits on the size and number of resources....
 ***************************************************************************/
/* The number of sections with independent log level control */
#define LOGIT_NROWS  (12)

/* Output types: stderr and syslog.  Remember that you must not
   let the program daemonize if you want to use stderr */
#define LOGIT_STDERR   (1)
#define LOGIT_SYSLOG   (2)

/* The maximum number of char in the "format" section of a log msg */
#define MX_LOG_FORMAT  (250)


/***************************************************************************
 *  - Data structures
 *    The structure to use in the table of sections and thresholds.
 ***************************************************************************/




/***************************************************************************
 *  - Function prototypes
 ***************************************************************************/




/***************************************************************************
 *  - Variable allocation and initialization
 ***************************************************************************/
char  Logit_Format[MX_LOG_FORMAT]; // where to prepend file name and line #


/* Allocate and give compile-time default to the logit table */
LOGIT Logit[LOGIT_NROWS] = {
  {"main", LOG_WARNING, LOGIT_SYSLOG, "MN, covers main.c"},
  {"select", LOG_WARNING, LOGIT_SYSLOG, "SL, select() loop"},
  {"timers", LOG_WARNING, LOGIT_SYSLOG, "TM, timers"},
  {"config", LOG_WARNING, LOGIT_SYSLOG, "CF, configuration"},
  {"", -1, 0, ""},
  {"", -1, 0, ""},
  {"", -1, 0, ""},
  {"", -1, 0, ""},
  {"", -1, 0, ""},
  {"", -1, 0, ""},
  {"", -1, 0, ""},
  {"", -1, 0, ""},
};

COLDEF   LogitColumns[] = {
  {
      "Logit",                  // the table name
      "section",                // the column name
      RTA_STR,                  // data type
      LOGIT_NAME_LEN,           // number of bytes
      offsetof(LOGIT, sect),    // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The name of the section of code covered by this log level.  "
      "The sections may be based on file names, such as 'mail.c', "
      "or may refer to logical blocks of code.  Remember that the "
      "logit() call takes and integer to describe the section.  "
      "That integer is the zero-indexed row number in this table."
  },
  {
      "Logit",                  // the table name
      "threshold",              // the column name
      RTA_INT,                  // data type
      sizeof(int),              // number of bytes
      offsetof(LOGIT, thres),   // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The threshold.  Logit() calls with a 'level' parameter numerically "
      "below or equal to this value are logged.  Calls to logit() with a "
      "level numerically greater than this value are not logged."
  },
  {
      "Logit",                  // the table name
      "output",                 // the column name
      RTA_INT,                  // data type
      sizeof(int),              // number of bytes
      offsetof(LOGIT, output),  // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The destinations of the logged message.  A zero suppresses "
      "output.  A '1' sends output to stderr.  A '2' sends output "
      "to syslog, and a '3' sends output to both.  The logit.h "
      "file has defines for LOGIT_STDERR and LOGIT_SYSLOG.  You "
      "should use these defines in your code."
  },
  {
      "Logit",                  // the table name
      "comment",                // the column name
      RTA_STR,                  // data type
      LOGIT_COMMENT_LEN,        // number of bytes
      offsetof(LOGIT, comment), // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "A comment to describe what code is covered in this section "
      "if it is not obvious from the section name."
  }
};

TBLDEF   LogitTable = {
  "Logit",                      // table name
  &Logit,                       // address of table
  sizeof(LOGIT),                // length of each row
  LOGIT_NROWS,                  // number of rows
  (void *) 0,                   // iterator callback
  (void *) 0,                   // iterator callback data
  LogitColumns,                 // array of column defs
  sizeof(LogitColumns) / sizeof(COLDEF), // the number of columns
  "logit.sql",                  // save file
  "A table giving the names of sections of code and giving the "
  "log level threshold for each section."
};




/***************************************************************
 * logitInit():  - Initialize all internal variables and tables
 * of the logging/debugging control
 * 
 * Input:  None
 * Return: None
 **************************************************************/
void     logitInit()
{
  rta_add_table(&LogitTable);
}



/***************************************************************
 * logit():  - Log a message to stderr or syslog.   The message
 * is logged if the log level passed is numerically less than or
 * equal to the threshold in the LOGIT table row specified by the
 * section ID in the call.  The file name and line number are passed
 * as part of the call too.
 *   The last parameters to the call are a format string and a va_arg
 * list of variables to print.
 *   
 * 
 * Return: nothing.
 **************************************************************/
void logit(
  int level,           // Logged if threshold is higher or equal
  int sectID,          // Index into the LOGIT table
  char *format,        // Log message printf string
  ...)                 // Parameters for the printf string
{
  va_list  args;
  int      priority;   // truncated priority passed to syslog

  if ((sectID >= LOGIT_NROWS) || (sectID < 0)) {
    /* Log this invalid call?  */
    return;
  }

  va_start(args, format);

  /* Output to stderr if specified */
  if (Logit[sectID].output & LOGIT_STDERR) {
    vfprintf(stderr, format, args);
  }

  /* Output to syslog if specified */
  if (Logit[sectID].output & LOGIT_SYSLOG) {
    priority = (level <= LOG_INFO) ? level : LOG_INFO;
    vsyslog(priority, format, args);
  }

  va_end(args);
}



/*****************************************************************************
 * void logitSetEntry() - assign an entry in the Logit table
 *
 * Output:    none
 * Affects:   none
 *****************************************************************************/
void logitSetEntry(
  int row,                 // row to set the values for
  char *sect,              // the section name
  int thres,               // the logging threshold
  int output,              // output  : 0=none, 1=stderr, 2=syslog, 3=both
  char *comment)           // a long description
{
	if ( (row >= 0) && (row < LOGIT_NROWS) ) {
		strncpy(Logit[row].sect, sect, LOGIT_NAME_LEN);
        Logit[row].sect[LOGIT_NAME_LEN] = (char) 0;
		strncpy(Logit[row].comment, comment, LOGIT_COMMENT_LEN);
        Logit[row].comment[LOGIT_COMMENT_LEN] = (char) 0;
		Logit[row].output = output;
		Logit[row].thres = thres;
	}
}


