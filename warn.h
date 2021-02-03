#ifndef XMS_WARN_H
#define XMS_WARN_H

#include "mhd.h"

extern void
warn (struct MHD_Connection *connection, const char *fmt, ...);

#endif /* XMS_WARN_H */
