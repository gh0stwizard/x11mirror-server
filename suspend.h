#ifndef _SUSPEND_H
#define _SUSPEND_H

#include "mhd.h"
#include "contexts.h"


extern void
init_suspend_pool (void);

extern void
resume_all_connections (struct MHD_Daemon *daemon, unsigned int mode);

extern void
suspend_connection (struct MHD_Connection *connection, request_ctx *req);

extern void
resume_next (struct MHD_Daemon *daemon, unsigned int mode);

#endif /* _SUSPEND_H */
