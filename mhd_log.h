#ifndef XMS_MHD_LOG_H
#define XMS_MHD_LOG_H

#include "mhd.h"

extern void
mhd_warn (struct MHD_Connection *connection, const char *fmt, ...);

extern void
mhd_note (struct MHD_Connection *connection, const char *fmt, ...);

extern void
mhd_info (struct MHD_Connection *connection, const char *fmt, ...);

extern void
mhd_error (struct MHD_Connection *connection, const char *fmt, ...);

extern void
mhd_debug (struct MHD_Connection *connection, const char *fmt, ...);

#endif /* XMS_MHD_LOG_H */
