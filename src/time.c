/**
 * Copyright 2017 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "time.h"
#include "aker_log.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

/* See time.h for details. */
time_t convert_unix_time_to_weekly(time_t unixtime)
{
    time_t seconds_since_sunday_midnght;
    time_t t = unixtime;
    struct tm ts;

    ts = *localtime(&t);

    seconds_since_sunday_midnght = (ts.tm_wday * 24 * 3600) +
            (ts.tm_hour * 3600) +
            (ts.tm_min * 60) +
            ts.tm_sec;


    return seconds_since_sunday_midnght;
}

/* See time.h for details. */
time_t get_unix_time(void)
{
    #define SLEEP_TIME 5
    struct timespec tm;
    time_t unix_time = 0;

    while( 1 ) {
        if( 0 == clock_gettime(CLOCK_REALTIME, &tm) ) {
            unix_time = tm.tv_sec; // ignore tm.tv_nsec
            break;
        }
        sleep(SLEEP_TIME);
    }

    return unix_time;
}

int set_unix_time_zone (const char *time_zone)
{
   struct tm *mt;
   time_t mtt;
   char ftime[10];
   char ftime2[10];
   int rv = 0;

   if(strcmp(time_zone, "US/Pacific") != 0)
   {
	setenv("TZ", time_zone, 1);
	tzset();
   }
   mtt = time(NULL);
   mt = localtime(&mtt);
   debug_info("mt->tm_zone = %s\n", mt->tm_zone);
   debug_info("mt->tm_zone[0] is %c mt->tm_isdst %ld\n", mt->tm_zone[0], mt->tm_isdst);
   if (0 != mt->tm_zone[0]) {
       strftime(ftime,sizeof(ftime),"%Z %H%M",mt);
       debug_info("ftime is %s\n", ftime);
   } else {
        debug_error("error case set unix time\n");
        strftime(ftime2,sizeof(ftime2),"%Z %H%M",mt);
	debug_info("ftime2 error case is %s\n", ftime2);
       strftime(ftime,sizeof(ftime),"nil %H%M",mt);
       debug_error("set_unix_time_zone() error, TZ = %s\n", time_zone);
   }

   debug_info("time_zone: %s is %s\n", time_zone, ftime);
   
   return rv;
}
/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/* none */
