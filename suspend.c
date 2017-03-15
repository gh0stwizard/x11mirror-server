#include "mhd.h"
#include "suspend.h"
#include "vector.h"
#include "common.h"
#include "warn.h"
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include <limits.h>


/* we have one global pool, mutex support implemented in vector.c */
static VECTOR *pool;


static void
run_daemon (struct MHD_Daemon *daemon, unsigned int mode);

static void
update_current_connection_timeout (struct MHD_Connection *connection);

static unsigned int
get_prev_connection_timeout (struct MHD_Connection *connection);


/* ------------------------------------------------------------------ */


static void
run_daemon (struct MHD_Daemon *daemon, unsigned int mode)
{
	struct timeval tv = { 0, 100 * 1000 };
	MHD_socket max = 0;
	fd_set rs, ws, es;

	debug ("! run_daemon\n");

	FD_ZERO (&rs);
	FD_ZERO (&ws);
	FD_ZERO (&es);

#if MHD_VERSION >= 0x00095100
	if (mode & MHD_USE_EPOLL) {
#else
	if (mode & MHD_USE_EPOLL_LINUX_ONLY) {
#endif
		MHD_run (daemon);
		return;
	}

	if (MHD_get_fdset (daemon, &rs, &ws, &es, &max) != MHD_YES)
		die ("! Fatal error: MHD_get_fdset failed: %s\n",
			strerror (errno));

	if (select (max + 1, &rs, &ws, &es, &tv) == -1) {
		if (errno != EINTR)
			die ("! Fatal error: select: %s\n", strerror (errno));
	}

	MHD_run_from_select (daemon, &rs, &ws, &es);
}


extern void
init_suspend_pool (void)
{
	pool = vector_new ();

	if (pool == NULL)
		die ("failed to initialize suspend pool");
}


extern void
resume_all_connections (struct MHD_Daemon *daemon, unsigned int mode)
{
	size_t total, i;
	void *entry;	
	struct MHD_Connection *connection;
#if MHD_VERSION >= 0x00095213
	(void) daemon;
	(void) mode;
#endif

	total = vector_count (pool);

	if (total == 0)
		return;

	debug ("* Resuming all %zu connections\n", total);

	/* TODO: vector_get + vector_reset in one call */
	for (i = 0, entry = NULL; i < total; i++) {
		if (vector_get (pool, i, &entry)) {
			if (entry == NULL)
				continue;
			connection = (struct MHD_Connection *)entry;
			warn (connection, "resuming");
			MHD_resume_connection (connection);
			/* restore prev. connection timeout */
			MHD_set_connection_option (
				connection,
				MHD_CONNECTION_OPTION_TIMEOUT,
				get_prev_connection_timeout (connection));
		}
		else
			die ("failed to get connection entry");
	}

	if (vector_count (pool) == total) {
		if (! vector_reset (pool))
			die ("failed to purge pool");
	}

#if MHD_VERSION < 0x00095213
	run_daemon (daemon, mode);
#endif
}


extern void
resume_connection_index (size_t index, struct MHD_Daemon *daemon, unsigned int mode)
{
	static void *entry;
	static struct MHD_Connection *connection;
#if MHD_VERSION >= 0x00095213
	(void) daemon;
	(void) mode;
#endif

	if (vector_delete (pool, index, &entry)) {
		connection = (struct MHD_Connection *) entry;
#if defined(_DEBUG)
		warn (connection, "resume");
#endif
		MHD_resume_connection (connection);
#if MHD_VERSION < 0x00095213
		run_daemon (daemon, mode);
#endif
	}
}

extern void
resume_next (struct MHD_Daemon *daemon, unsigned int mode)
{
	size_t total;
	void *entry;	
	struct MHD_Connection *connection;
#if MHD_VERSION >= 0x00095213
	(void) daemon;
	(void) mode;
#endif

	total = vector_count (pool);

	if (total == 0)
		return;

	debug ("* Resuming next connection...\n");

	if (! vector_delete (pool, 0, &entry)) {
		debug ("! Fatal error: vector_delete: %s\n",
			strerror (vector_get_errno (pool)));
		return;
	}

	if (entry != NULL) {
		connection = (struct MHD_Connection *)entry;
		warn (connection, "resumed");
		MHD_resume_connection (connection);
		/* restore prev. connection timeout */
		MHD_set_connection_option (
			connection,
			MHD_CONNECTION_OPTION_TIMEOUT,
			get_prev_connection_timeout (connection));
	}
	else
		debug ("! Failed to get connection entry\n");

#if MHD_VERSION < 0x00095213
	run_daemon (daemon, mode);
#endif
}


extern size_t
suspend_connection_old (struct MHD_Connection *connection)
{
	size_t index;

	MHD_suspend_connection (connection);

	if (! vector_add (pool, (void *) connection))
		die ("failed to add connection to pool (%d)",
			vector_get_errno (pool));

	index = vector_count (pool) - 1;

#if defined(_DEBUG)
	warn (connection, "suspend");
#endif

	return index;
}


extern void
suspend_connection (struct MHD_Connection *connection, request_ctx *req)
{
/*
	void *timeout;
*/

	update_current_connection_timeout (connection);

	MHD_set_connection_option (
		connection,
		MHD_CONNECTION_OPTION_TIMEOUT,
		0);

/*
	MHD_get_connection_option (
		connection,
		MHD_CONNECTION_OPTION_TIMEOUT,
		&timeout);

	warn (connection, "new timeout: %u", timeout);
*/

	MHD_suspend_connection (connection);

	if (! vector_add (pool, (void *) connection))
		die ("failed to add connection to pool (%d)",
			vector_get_errno (pool));

	req->suspend_index = vector_count (pool) - 1;

#if defined(_DEBUG)
	warn (connection, "suspend");
#endif
}


static void
update_current_connection_timeout (struct MHD_Connection *connection)
{
	const union MHD_ConnectionInfo *ci_ctx, *ci_timeout;
	socket_ctx *sc;
	unsigned int timeout;


	ci_ctx = MHD_get_connection_info
		(connection, MHD_CONNECTION_INFO_SOCKET_CONTEXT);

	ci_timeout = MHD_get_connection_info
			(connection, MHD_CONNECTION_INFO_CONNECTION_TIMEOUT);

	if ((ci_ctx != NULL) && (ci_timeout != NULL)) {
		sc = *(socket_ctx **) ci_ctx;
		timeout = *(unsigned int *) ci_timeout;
/*
		warn (connection, "timeout: current = %u, prev. = %u",
			timeout, sc->prev_timeout);
*/
		sc->prev_timeout = timeout;
	}
	else
		warn (connection, "failed to update current connection timeout");
}


static unsigned int
get_prev_connection_timeout (struct MHD_Connection *connection)
{
	const union MHD_ConnectionInfo *ci;
	socket_ctx *sc;


	ci = MHD_get_connection_info
		(connection, MHD_CONNECTION_INFO_SOCKET_CONTEXT);

	if (ci != NULL) {
		sc = *(socket_ctx **) ci;
		return sc->prev_timeout;
	}

	warn (connection, "failed to get prev. connection timeout");

	return 0; /* must be 0 */
}
