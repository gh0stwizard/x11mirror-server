#ifndef _SUSPEND_H
#define _SUSPEND_H

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

#endif /* _SUSPEND_H */
