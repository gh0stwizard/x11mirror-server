#ifndef XMS_COMMON_H
#define XMS_COMMON_H


#if defined (_DEBUG)
#include "timeinfo.h"
#define debug(...) do {\
	char __d[DATE_SIZE];\
	char *__dp = __d;\
	if (get_current_time_string (&__dp, DATE_SIZE))\
		fprintf (stderr, "%s ", __d);\
	fprintf (stderr,  __VA_ARGS__);\
} while (0)
#else
#define debug(...) do { /* nop */ } while (0)
#endif

extern void
die (const char *fmt, ...);

#endif /* XMS_COMMON_H */
