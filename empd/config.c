/***************************************************************************
 *                        config.c
 *  copyright            : (C) 2004 by Bob Smith
 *
 *   This file has the configuration 
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


static char const Version_config_c[] = "$Id$";



#include <stdio.h>
#include <stddef.h>             /* for 'offsetof' */
#include <stdarg.h>             /* for va_arg */
#include <limits.h>             /* for PATH_MAX */
#include <string.h>             /* for strncpy() */
#include <stdlib.h>             /* for exit() */
#include <unistd.h>             /* for getopt() */
#include <pwd.h>                /* passwd struct */
#include <errno.h>              /* for errno */
#include <signal.h>             /* for kill() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>              /* for open() */
#include <sys/file.h>           /* for flock() */
#include <sys/syslog.h>         /* for log levels */
#include "/usr/local/include/rta.h"
#include "empd.h"


/***************************************************************************
 * Overview:
 *   The configuration file for the empty daemon has the following item:
 * - daemonize:  y or n.
 * - RTA port:   integer.  The TCP port for RTA connections
 * - RTA addr:   The IP address for RTA conns.  default is 127.0.0.1
 * - config dir: path.  Desired configuration directory.  
 * - gid:        integer or group name.
 * - uid:        integer or user name.
 * - pidfile:    file name.  No pidfile if null.
 * - version:    string.  Version of this daemon.
 * - trigger:    integer.  Set to 1 to open RTA port.
 *
 *   We have a table with columns for each of the above.  There is a
 * priority to the configuration values.  Listed from lowest priority
 * to highest are
 *    - built-in hard-coded #define values
 *    - config file values
 *    - command line values
 *    - run-time values set by RTA
 *
 *   We do not want a value specified on the command line to be saved to
 * the config file inadvertently so we must have two rows for these values,
 * one row for the config file values and another for the working/cmd line
 * values.  At init time the hard-coded values are written into both rows.
 * Then we look at the command line for a '-d' option to specify a config
 * directory.  We then load the table with values from the config file.
 * Lastly, we again examine the command line for configuration parameters.
 * Command line parameters go only in the second row, the "working" row.
 * The first row has the values from the config file and is called the
 * "saved" row.  Any values written from the RTA (run time) interface is
 * written to both rows and is also saved in the config table's save file.
 *
 *   The following are examined only when the program starts and changes
 * from the run time interface have no effect until the next invocation:
 *    - daemonize, directory, gid, uid, pidfile
 * The following configuration parameters are tracked while the program
 * runs:
 *    - RTA port, RTA addr
 *
 ***************************************************************************/



/***************************************************************************
 *  - Limits and defines
 *    Limits on the size and number of resources....
 ***************************************************************************/
/* The row with the values from the config file */
#define ED_CONFIG       0

/* The row with the command line/working values */
#define ED_WORKING      1

/* String lengths used in the configuration table */
#define MAX_IP          16
#define MAX_GID         30
#define MAX_UID         30
#define MAX_VER         30

#define DEF_DAEMONIZE   1
#define DEF_RTA_PORT    8888
#define DEF_RTA_ADDR    "127.0.0.1"
#define DEF_CONFDIR     "/usr/local/empd"
#define DEF_ED_GID      ""
#define DEF_ED_UID      ""
#define DEF_PIDFILE     "/usr/local/empd/empd.pid"
#define ED_VERSION      "0.1.0"



/***************************************************************************
 *  - Function prototypes
 ***************************************************************************/
void     do_trigger(char *, char *, char *, void *, int, void *);
void     do_command_line(int, char *[]);
void     usage(char *progname);
void     do_set_uid();
void     do_set_gid();
void     do_pidfile();
extern void     open_rta_port(int rta_port, char *rta_addr);


/***************************************************************************
 *  - Data structures
 *    The structure to use to hold the configuration values.
 ***************************************************************************/
typedef struct
{
  int  daemonize;         // daemonize if 1, stay in foreground if 0
  int  rta_port;          // RTA port for the ui
  char rta_addr[MAX_IP];  // The bind address for the RTA port
  char confdir[PATH_MAX]; // Desired config directory. 
  char gid[MAX_GID];      // Group ID or name
  char uid[MAX_UID];      // User ID or name
  char pidfile[PATH_MAX]; // PID file name
  char version[MAX_VER];  // the version of this program
  int  trigger;           // set to 1 to open rta_port on rta_addr
}
CONFIG;



/***************************************************************************
 *  - Variable allocation and initialization
 ***************************************************************************/
CONFIG Config[2];

COLDEF   ConfigColumns[] = {
  {
      "Config",                  // the table name
      "daemonize",                // the column name
      RTA_INT,                  // data type
      sizeof(int),     // number of bytes
      offsetof(CONFIG, daemonize),    // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "Whether or not to daemonize the program.  A 'y' cause the "
      "program to fork(), change group ID and user ID, set the "
      "working directory, fork again, and close stdin, stdout, and "
      "stderr.  The daemonize parameter is examined only when the "
      "program starts and is ignored otherwise."
  },
  {
      "Config",                  // the table name
      "rta_port",                // the column name
      RTA_INT,                  // data type
      sizeof(int),           // number of bytes
      offsetof(CONFIG, rta_port),    // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The listen TCP port for RTA/Postgres connections from the "
      "user interface programs.  This value can be changed after "
      "the program starts but care should be takes as this may "
      "confuse any running UI programs."
  },
  {
      "Config",                  // the table name
      "rta_addr",                // the column name
      RTA_STR,                  // data type
      MAX_IP,           // number of bytes
      offsetof(CONFIG, rta_addr),    // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The bind address for the RTA port.  If left blank the "
      "port is bound to all IP interfaces.  Be aware that "
      "changing this from its default value of 127.0.0.1 "
      "may expose your table to everyone on the network. "
      "If you care about security, leave this at 127.0.0.1."
  },
  {
      "Config",                  // the table name
      "confdir",                // the column name
      RTA_STR,                  // data type
      PATH_MAX,           // number of bytes
      offsetof(CONFIG, confdir),    // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The desired configuration directory for the daemon.  An empty "
      "confdir causes the program to use the directory from "
      "which the program was started."
  },
  {
      "Config",                  // the table name
      "gid",                // the column name
      RTA_STR,                  // data type
      MAX_GID,           // number of bytes
      offsetof(CONFIG, gid),    // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The group ID to be used when the program runs.  Leave blank if "
      "you want to use the gid of the user invoking the daemon.  The gid "
      "can be specified as either a name or as an integer.  This "
      "value is examined only when the daemon starts."
  },
  {
      "Config",                  // the table name
      "uid",                // the column name
      RTA_STR,                  // data type
      MAX_UID,           // number of bytes
      offsetof(CONFIG, uid),    // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The user ID to be used when the program runs.  Leave blank if "
      "you want to use the uid of the user invoking the daemon.  The uid "
      "can be specified as either a name or as an integer.  This "
      "value is examined only when the daemon starts."
  },
  {
      "Config",                  // the table name
      "pidfile",                // the column name
      RTA_STR,                  // data type
      PATH_MAX,           // number of bytes
      offsetof(CONFIG, pidfile),    // location in struct
      RTA_DISKSAVE,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The location of the pidfile.  The program will not start "
      "if this file exists and the PID specified in the "
      "file is a currently running process.  The PID file is "
      "over written with this process's PID if the old PID is "
      "no longer running.  This value is examined only when the "
      "daemon starts."
  },
  {
      "Config",                  // the table name
      "version",                // the column name
      RTA_STR,                  // data type
      MAX_VER,           // number of bytes
      offsetof(CONFIG, version),    // location in struct
      RTA_READONLY,             // flags
      (void (*)()) 0,           // called before read
      (void (*)()) 0,           // called after write
      "The version number of this daemon.  This value is read-only "
      "and is available at all times."
  },
  {
      "Config",                  // the table name
      "trigger",                // the column name
      RTA_INT,                  // data type
      sizeof(int),           // number of bytes
      offsetof(CONFIG, trigger),    // location in struct
      0,                        // flags
      (void (*)()) 0,           // called before read
      do_trigger,               // called after write
      "The 0-to-1 transition of trigger causes the opening, or "
      "re-opening of the RTA listen TCP socket on rta_port and "
      "rta_addr.  The variable trigger is immediately set back "
      "to 0 so you should never see it at 1."
  }
};

TBLDEF   ConfigTable = {
  "Config",                      // table name
  &Config,                       // address of table
  sizeof(CONFIG),                // length of each row
  2,                  // number of rows
  (void *) 0,                   // iterator callback
  (void *) 0,                   // iterator callback data
  ConfigColumns,                 // array of column defs
  sizeof(ConfigColumns) / sizeof(COLDEF), // the number of columns
  "Config.sql",                  // save file
  "The saved and current working configuration of the daemon"
};




/***************************************************************
 * configInit():  - Initialize all internal variables and tables.
 * 
 * Input:  int argc:      command line argument count
 *         char *argv[]:  array of command line arguments
 * Return: None
 **************************************************************/
void     configInit(int argc, char *argv[])
{
  /* Give the config table the hard coded defaults.  */
  Config[ED_CONFIG].daemonize = DEF_DAEMONIZE;
  Config[ED_CONFIG].rta_port = DEF_RTA_PORT;
  Config[ED_CONFIG].trigger = 0;
  strncpy(Config[ED_CONFIG].gid, DEF_ED_GID, MAX_GID);
  strncpy(Config[ED_CONFIG].pidfile, DEF_PIDFILE, PATH_MAX);
  strncpy(Config[ED_CONFIG].rta_addr, DEF_RTA_ADDR, MAX_IP);
  strncpy(Config[ED_CONFIG].uid, DEF_ED_UID, MAX_UID);
  strncpy(Config[ED_CONFIG].version, ED_VERSION, MAX_VER);
  strncpy(Config[ED_CONFIG].confdir, DEF_CONFDIR, PATH_MAX);


  /* Add the table to the RTA system.  This will overwrite 
     the hard-coded values with the values from the config file */
  rta_add_table(&ConfigTable);

  /* Now copy the saved configuration to the working config */
  (void) memcpy((void *) &Config[ED_WORKING],
                (void *) &Config[ED_CONFIG], sizeof(CONFIG));


  /* Get parameters from the command line.  */
  do_command_line(argc, argv);

  /* If a config directory is set, tell RTA about it */
  if (Config[ED_WORKING].confdir[0]) {
    if (rta_config_dir(Config[ED_WORKING].confdir)) {
      LOG(LOG_ERR, CF, E_Conf_Dir, Config[ED_WORKING].confdir);
      exit(-1);
    }
  }

  /* If daemonize is set, then make us a daemon */
  if (Config[ED_WORKING].daemonize)
    daemon(0, 0);

  /* If a GID is specified, then set GID (or die trying) */
  if (Config[ED_WORKING].gid[0])
    do_set_gid();

  /* If a UID is specified, then set UID (or die trying) */
  if (Config[ED_WORKING].uid[0])
    do_set_uid();

  /* If a PID file is specified, then save the PID (or die trying) */
  if (Config[ED_WORKING].pidfile[0])
    do_pidfile();


  /* Now open the RTA socket for the first time */
  open_rta_port(Config[ED_WORKING].rta_port, Config[ED_WORKING].rta_addr);
}



/***************************************************************
 * do_command_line():  - Process the command line and overwrite
 * the WORKING configuration with values from the command line.
 * 
 * Input:    int argc:      command line argument count
 *           char *argv[]:  array of command line arguments
 * Return:   None
 * Affects:  May update the WORKING row of Config
 **************************************************************/
void
do_command_line(int argc, char *argv[])
{
  char  cmdc;      // command line character
  int  tmp_int;    // a temporary integer

  optind = 0;      // reset the scan of the cmd line arguments
  optarg = argv[0];
  while ((cmdc = getopt(argc, argv, "a:bd:g:np:u:v")) != EOF)
  {
    switch ((char) cmdc)
    {
      case 'a':   // Bind Address
        strncpy(Config[ED_WORKING].rta_addr, optarg, MAX_IP);
        Config[ED_WORKING].rta_addr[MAX_IP - 1] = (char) 0;
        break;

      case 'b':  // Background (daemonize)
        Config[ED_WORKING].daemonize = 1;
        break;

      case 'd':  // Configuration Directory
        strncpy(Config[ED_WORKING].confdir, optarg, PATH_MAX);
        Config[ED_WORKING].confdir[PATH_MAX - 1] = (char) 0;
        break;

      case 'g':   // Effective Group ID
        strncpy(Config[ED_WORKING].gid, optarg, MAX_GID);
        Config[ED_WORKING].gid[MAX_GID - 1] = (char) 0;
        break;

      case 'n':  // No Background (daemonize)
        Config[ED_WORKING].daemonize = 0;
        break;

      case 'p':  // Bind Port
        if (sscanf(optarg, "%d", &tmp_int)) {
          Config[ED_WORKING].rta_port = tmp_int;
        }
        break;

      case 'u':   // Effective User ID
        strncpy(Config[ED_WORKING].uid, optarg, MAX_UID);
        Config[ED_WORKING].uid[MAX_UID - 1] = (char) 0;
        break;

      case 'v':  // Print version and exit
        printf("%s version: %s\n", argv[0], Config[ED_WORKING].version);
        exit(0);
        break;

      default:
        usage(argv[0]);
        exit(-1);
        break;
    }
  }
}


/***************************************************************
 * do_trigger(): - Callback on the 'trigger' column.
 *   Perform any changes needed by an update to the configurtaion
 * table.  Currently this means to open or re-open the RTA socket.
 * We only make the changes if this update is to the working row,
 * that is, the row with rowid == ED_WORKING.
 *
 * Input:        char *tbl     -- the table read (UIConns)
 *               char *col     -- the column read (cdur)
 *               char *sql     -- actual SQL of the command
 *               void *pr      -- points to row affected
 *               int   rowid   -- row number of row affected 
 *               void *poldrow -- points to row affected
 * Output:       none
 * Effects:      If set to a 1, we open the RTA listen socket
***************************************************************/
void
do_trigger(char *tbl, char *col, char *sql, void *pr, int rowid,
           void *poldrow)
{
  if(((CONFIG *)pr)->trigger == 0)
    return;

  ((CONFIG *)pr)->trigger = 0;    // reset the trigger

  if(rowid != ED_WORKING)         // only change if WORKING val changes
    return;

  /* re-open the RTA socket */
  open_rta_port(Config[ED_WORKING].rta_port, Config[ED_WORKING].rta_addr);
}


/***************************************************************
 * usage():  - prints program usage.
 * 
 * Input:  progname:  the name of the calling program
 * Return: None
 **************************************************************/
void
usage(char *progname)
{
  printf("%s: usage [options]\nOptions include:\n", progname);
  printf("\t[-a Bind_Address]\n");
  printf("\t[-b]  ...to put into background\n");
  printf("\t[-d config_directory]\n");
  printf("\t[-g Group_ID]\n");
  printf("\t[-n]  ...to leave daemon in foreground\n");
  printf("\t[-p Bind_Port]\n");
  printf("\t[-u User_ID]\n");
  printf("\t[-v]  ...to print version and exit\n");
}



/***************************************************************
 * do_set_uid():  - Set the uid for our daemon.  This is too
 * important to go wrong so we die if there are any problems.
 * 
 * Input:    UID string from Config[ED_WORKING]
 * Return:   None
 * Affects:  None
 **************************************************************/
void
do_set_uid()
{
  struct passwd *ppw;    // pointer to passwd struct
  int            uid;    // UID as an integer

  /* The UID in the Config table is a string and may contain
     either a number or a user name.  We test for a numeric
     value by trying a to-int conversion.  If that fails we
     do a passwd file look up on the name. */
  if (sscanf(Config[ED_WORKING].uid, "%d", &uid) != 1) {
    ppw = getpwnam(Config[ED_WORKING].uid);
    if (!ppw) {
      exit(-1);    // die on errors 
    }
    uid = ppw->pw_uid;
  }

  /* Got a numeric UID.  Set it */
  if (setuid(uid) != 0) {
    // There was a problem setting the uid.  Die on errors, so ...
    exit(-1);
  }
}



/***************************************************************
 * do_set_gid():  - Set the gid for our daemon.  This is too
 * important to go wrong so we die if there are any problems.
 * 
 * Input:    GID string from Config[ED_WORKING]
 * Return:   None
 * Affects:  None
 **************************************************************/
void
do_set_gid()
{
  struct passwd *ppw;    // pointer to passwd struct
  int            gid;    // GID as an integer

  /* The GID in the Config table is a string and may contain
     either a number or a user name.  We test for a numeric
     value by trying a to-int conversion.  If that fails we
     do a passwd file look up on the name. */
  if (sscanf(Config[ED_WORKING].gid, "%d", &gid) != 1) {
    ppw = getpwnam(Config[ED_WORKING].gid);
    if (!ppw) {
      exit(-1);    // die on errors 
    }
    gid = ppw->pw_gid;
  }

  /* Got a numeric UID.  Set it */
  if (setgid(gid) !=0 ) {
    // There was a problem setting the uid.  Die on errors, so ...
    exit(-1);
  }
}



/***************************************************************
 * do_pidfile():  - Save our PID to the pidfile or die trying.
 * If the pidfile already exists and has the PID of a running
 * process, we report that the program is already running and
 * exit.
 * 
 * Input:    PID file name from Config[ED_WORKING]
 * Return:   None
 * Affects:  None
 **************************************************************/
void
do_pidfile()
{
  FILE *pf;        // We use a FILE to use fscanf
  int   fd;        // File descriptor for pid file
  int   fpid;      // PID found in existing pidfile
  int   opid;      // Our PID

  pf = fopen(Config[ED_WORKING].pidfile, "r");
  if (pf) {
    if (fscanf(pf, "%d", &fpid)) {
      /* We've gotten a PID out of the file.  Is it running? */
      if (!(kill(fpid, 0) == -1 &&  errno == ESRCH)) {
        /* Looks like another daemon is running.  Exit. */
        (void) fclose(pf);
        LOG(LOG_ERR, CF, E_Not_Alone,fpid);
        exit(-1);
      }
    }
    /* stale PID file.  remove it */
    (void) fclose(pf);
    if (unlink(Config[ED_WORKING].pidfile) != 0) {
      /* Could not remove PID file.  Exit.  */
      LOG(LOG_ERR, CF, E_Pid_File, Config[ED_WORKING].pidfile);
      exit(-1);
    }
  }

  /* No PID file or a stale one has been removed.  Write a new one. */
  fd = creat(Config[ED_WORKING].pidfile, 0644);
  if (fd < 0) {
    LOG(LOG_ERR, CF, E_Pid_File, Config[ED_WORKING].pidfile);
    exit(-1);
  }

  /* get a file lock or die trying */
  if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
    LOG(LOG_ERR, CF, E_Pid_File, Config[ED_WORKING].pidfile);
    exit(-1);
  }

  opid = getpid();     // get our pid 

  /* Get a FILE pointer so we can use fprintf */
  pf = fdopen(fd, "w");
  if (!pf) {
    LOG(LOG_ERR, CF, E_Pid_File, Config[ED_WORKING].pidfile);
    exit(-1);
  }

  (void) fprintf(pf, "%d\n", opid);
  fflush(pf);

  (void) flock(fd, LOCK_UN);

  (void) close(fd);
}

 

