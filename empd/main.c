/***************************************************************************
 *                        main.c
 *  copyright            : (C) 2004 by Bob Smith
 *
 *   This file has the emtry point for the empty daemon program.  It also
 * handles all initialization.  The main loop of the daemon is in select.c.
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


static char const Version_main_c[] = "$Id: main.c,v 1.2 2004/04/05 16:29:11 graham Exp $";


#include <stdio.h>                     /* for EOF */
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>                    /* for getopt() */
#include <syslog.h>                    /* for log levels */
#include "rta.h"
#include "empd.h"



/***************************************************************************
 *  - Limits and defines
 *    Limits on the size and number of resources....
 ***************************************************************************/


/***************************************************************************
 *  - Function prototypes
 ***************************************************************************/


/***************************************************************************
 *  - Data structures
 ***************************************************************************/

/***************************************************************************
 *  - Variable allocation and initialization
 ***************************************************************************/



/***************************************************************************
 * main(): - Main entry point for the empty daemon.
 *   This routine does command line parsing, initializes all
 * of the tables, and goes into the main select() loop.
 *
 * Input:        argc, argv
 * Output:       0 on normal exit, -1 on error exit with errno set
 * Effects:      manager connection table in the UI
 ***************************************************************************/
int
main(int argc, char **argv)
{
  int      cmdc;       /* command line character option */

  /* Set configuration directory before loading config files */
  opterr = 0;   // no error report of unknown options
  while ((cmdc = getopt(argc, argv, "a:bd:g:np:u:v")) != EOF)
  {
    switch ((char) cmdc)
    {
      case 'd':
        if (rta_config_dir(optarg)) {
          LOG(LOG_ERR, CF, E_Conf_Dir, optarg);
          exit(-1);
        }
        break;

      default:
        //  Ignore all other options at this point
        break;
    }
  }


  /* Initialize the FD sets in the select loop.  */
  selectInit();   // do first to init FD sets

  /* Init the daemon configuration.  Process command line for 
     additional configuration parameters.  This is where all
     hard work of daemon init is done.  */
  configInit(argc, argv);

  /* Initialize the debug and logging stuff */
  logitInit();

  /* Initialize the timers  */
  timerInit();

  /* Initialize the user interface */
  uiInit();

  /* Initialize the "real" application */
  appInit();

  /* enter the select loop, from which we do not return */
  doSelect();

  exit(0);
}

