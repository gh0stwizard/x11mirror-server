#ifndef _SUSPEND_H
#define _SUSPEND_H

#include "mhd.h"


extern void
init_suspend_pool (void);

extern void
resume_all_connections (struct MHD_Daemon *daemon);

extern void
resume_connection (size_t index, struct MHD_Daemon *daemon);

extern size_t
suspend_connection (struct MHD_Connection *connection);


#endif /* _SUSPEND_H */
