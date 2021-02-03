#ifndef XMS_SUSPEND_H
#define XMS_SUSPEND_H

#include "mhd.h"
#include "contexts.h"


extern void
init_suspend_pool (void);

extern void
free_suspend_pool (void);

extern void
resume_all_connections (void);

extern void
suspend_connection (struct MHD_Connection *connection);

extern void
resume_next (void);

#endif /* XMS_SUSPEND_H */
