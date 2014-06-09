/***************************************************************************
 *  Put your application code here
 ***************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <time.h>               /* for time() function */
#include <sys/time.h>           /* for timezone struct */
#include <syslog.h>             /* for log levels */
#include <stdlib.h>             /* for malloc() */
#include <errno.h>
#include "/usr/local/include/rta.h"
#include "empd.h"
#include "app.h"


static char const Version_app_c[] = "$Id$";




/***************************************************************************
 *  - Limits and defines
 *    Limits on the size and number of resources....
 ***************************************************************************/


/***************************************************************************
 *  - Data structures
 ***************************************************************************/


/***************************************************************************
 *  - Variable allocation and initialization
 ***************************************************************************/


/***************************************************************************
 *  - Function prototypes
 ***************************************************************************/
void timercb(void *, void *);





/***************************************************************
 * appInit():  - Initialize all internal variables and tables.
 * 
 * Input:  None
 * Return: None
 **************************************************************/
void     appInit()
{
  struct timeval  tv;     // timeval struct to hold "now"
  struct timezone tz;     // timezone struct to how "now"
  void *killit;

  /* Get "now" */
  if (gettimeofday(&tv, &tz)) {
    printf("Can't get time of day!\n");
  }

  printf("started at %d.%03d\n", (int) tv.tv_sec, 
          ((int)tv.tv_usec / 1000));

 killit =  add_timer(ED_ONESHOT, 11000, timercb, (void *)__LINE__);
 killit =  add_timer(ED_ONESHOT, 26000, timercb, (void *)__LINE__);
 killit =  add_timer(ED_ONESHOT, 33000, timercb, (void *)__LINE__);
 killit =  add_timer(ED_PERIODIC, 2000, timercb, (void *)__LINE__);
 killit =  add_timer(ED_PERIODIC, 4000, timercb, (void *)__LINE__);
 killit =  add_timer(ED_PERIODIC, 8000, timercb, (void *)__LINE__);

  //remove_timer(killit);

}






void timercb(void *handle, void *cbd)
{
  struct timeval  tv;     // timeval struct to hold "now"
  struct timezone tz;     // timezone struct to how "now"

  /* Get "now" */
  if (gettimeofday(&tv, &tz)) {
    printf("Can't get time of day!\n");
  }

  printf("%d:  at %d.%03d\n", (int) cbd, (int) tv.tv_sec, 
         ((int)tv.tv_usec / 1000));
}



