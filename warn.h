#ifndef _WARN_H
#define _WARN_H

#include "mhd.h"

extern void
warn (struct MHD_Connection *connection, const char *fmt, ...);

#endif /* _WARN_H */
