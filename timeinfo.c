#include <stdio.h>
#include <string.h>
#ifndef _POSIX_C_SOURCE
/* clock_gettime */
#define _POSIX_C_SOURCE 199309L
#endif
#include <time.h>
#include <errno.h>
#include "timeinfo.h"


/* a precision of floating part of seconds */
#define USEC_SIZE 4
#define NSEC_SIZE 9


extern int
get_current_time_string (char *out[], size_t out_len)
{
	struct timespec tp;
	struct tm *tmp;
	size_t len, i;
	char usec[USEC_SIZE + 1];
	char *usecp;
	char nsec[NSEC_SIZE + 1];


	if (clock_gettime (CLOCK_REALTIME, &tp) == -1) {
		fprintf (stderr, "clock_gettime: %s\n",
			strerror (errno));
		return 0;
	}

	tmp = localtime (&tp.tv_sec);

	if (tmp == NULL) {
		fprintf (stderr, "localtime: %s\n",
			strerror (errno));
		return 0;
	}

	if (strftime (*out, out_len, TIME_FMT, tmp) == 0) {
		fprintf (stderr, "strftime: %s\n",
			strerror (errno));
		return 0;
	}

	/* prepend zeros */
	snprintf (nsec, NSEC_SIZE + 1, "%li", tp.tv_nsec);
	len = strlen (nsec);
	memmove (nsec + NSEC_SIZE - len, nsec, len);
	for (i = 0, usecp = nsec; i < NSEC_SIZE - len; i++, usecp++)
		*usecp = '0';
	nsec[NSEC_SIZE] = '\0';
	
	/* cut */
	snprintf (usec, USEC_SIZE + 1, "%s", nsec);
	
	len = strlen (*out);
	snprintf (*out + len, DATE_SIZE - len, ".%s", usec);

	return len;
}
