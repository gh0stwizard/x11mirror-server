#ifndef XMS_COMMON_H
#define XMS_COMMON_H

#include "vlogger.h"

#define info(...) do { \
	vlogger_log (VLOGGER_INFO, __VA_ARGS__); \
} while (0)

#define warn(...) do { \
	vlogger_log (VLOGGER_WARN, __VA_ARGS__); \
} while (0)

#define note(...) do { \
	vlogger_log (VLOGGER_NOTE, __VA_ARGS__); \
} while (0)

#define error(...) do { \
	vlogger_log (VLOGGER_ERROR, __VA_ARGS__); \
} while (0)

#define debug(...) do { \
	vlogger_log (VLOGGER_DEBUG, __VA_ARGS__); \
} while (0)

#define fatal(...) do { \
	vlogger_log (VLOGGER_FATAL, __VA_ARGS__); \
} while (0)

#define die(...) do { \
	fprintf (stderr, __VA_ARGS__); \
	exit (EXIT_FAILURE); \
} while (0)

#endif /* XMS_COMMON_H */
