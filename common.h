#ifndef XMS_COMMON_H
#define XMS_COMMON_H

#include "vlogger.h"

#if defined (_DEBUG)
#define debug(...) do { \
	vlogger_log (VLOGGER_DEBUG, __VA_ARGS__); \
} while (0)
#else
#define debug(...) do { /* nop */ } while (0)
#endif

#define fatal(...) do { \
	vlogger_log (VLOGGER_FATAL, __VA_ARGS__); \
} while (0)

#define die(...) do { \
	fprintf (stderr, __VA_ARGS__); \
	exit (EXIT_FAILURE); \
} while (0)

#endif /* XMS_COMMON_H */
