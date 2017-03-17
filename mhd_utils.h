#ifndef _MHD_UTILS_H
#define _MHD_UTILS_H

#include "mhd.h"

#if 0
extern int
get_daemon_flags (struct MHD_Connection *connection);
#endif

extern unsigned int
get_connection_timeout (struct MHD_Connection *connection);

#endif /* _MHD_UTILS_H */
