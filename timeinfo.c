#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "timeinfo.h"


/* a precision of floating part of seconds */
#define MAX_DIGITS 6
#define NSEC_SIZE 9


extern int
get_current_time_string (char *out[], size_t out_len)
{
	struct timespec tp;
	struct tm *tmp;
	size_t len;
	char nsec[NSEC_SIZE + 1];
	long x;


	if (clock_gettime (CLOCK_REALTIME, &tp) == -1) {
		fprintf (stderr, "%s: clock_gettime: %s\n",
			__func__, strerror (errno));
		return 0;
	}

	tmp = localtime (&tp.tv_sec);

	if (tmp == NULL) {
		fprintf (stderr, "%s: localtime: %s\n",
			__func__, strerror (errno));
		return 0;
	}

	/* fill in a date to 'out' first */
	if (strftime (*out, out_len, TIME_FMT, tmp) == 0) {
		fprintf (stderr, "%s: strftime: %s\n",
			__func__, strerror (errno));
		return 0;
	}

	/* prepend zeros */
	memset (nsec, '0', NSEC_SIZE - 1);
	for (len = 0, x = tp.tv_nsec; x != 0; len++, x /= 10L);
	snprintf (nsec + NSEC_SIZE - len, len + 1, "%li", tp.tv_nsec);
	
	/* cut last digits and append the result */
	len = strlen (*out);
	/* MAX_DIGITS + '.' + '\0' */
	return snprintf (*out + len, MAX_DIGITS + 2, ".%s", nsec);
}
