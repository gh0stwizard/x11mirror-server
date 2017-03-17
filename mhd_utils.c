#include "mhd_utils.h"
#include "common.h"
#include "warn.h"


#if 0
extern int
get_daemon_flags (struct MHD_Connection *connection)
{
	const union MHD_ConnectionInfo *ci;
	struct MHD_Daemon *daemon;
	const union MHD_DaemonInfo *di;


	ci = MHD_get_connection_info (connection, MHD_CONNECTION_INFO_DAEMON);

	if (ci != NULL) {
		daemon = *(struct MHD_Daemon **) ci;
		di = MHD_get_daemon_info (daemon, MHD_DAEMON_INFO_FLAGS);

		if (di != NULL)
			return *(int *)di;
	}

	return -1;
}


extern unsigned int
get_connection_timeout (struct MHD_Connection *connection)
{
	const union MHD_ConnectionInfo *ci;


	ci = MHD_get_connection_info
			(connection, MHD_CONNECTION_INFO_CONNECTION_TIMEOUT);

	if (ci != NULL)
		return *(unsigned int *) ci;

	warn (connection, "ERROR: get_connection_timeout");

	return 0;
}

#endif /* #if 0 */
