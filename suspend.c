#include "mhd.h"
#include "suspend.h"
#include "vector.h"
#include "common.h"
#include "warn.h"
#include <errno.h>

static VECTOR *pool;


static void
run_daemon (struct MHD_Daemon *daemon);


static void
run_daemon (struct MHD_Daemon *daemon) {
	struct timeval tv;
	struct timeval *tvp;
	fd_set rs;
	fd_set ws;
	fd_set es;
	MHD_socket max;
	MHD_UNSIGNED_LONG_LONG mhd_timeout;

	max = 0;
	FD_ZERO (&rs);
	FD_ZERO (&ws);
	FD_ZERO (&es);

	if (MHD_get_fdset (daemon, &rs, &ws, &es, &max) != MHD_YES)
		die ("Fatal error: MHD_get_fdset failed");

	if (MHD_get_timeout (daemon, &mhd_timeout) == MHD_YES) {
		tv.tv_sec = mhd_timeout / 1000;
		tv.tv_usec = (mhd_timeout - (tv.tv_sec * 1000)) * 1000;
		tvp = &tv;
	}
	else
		tvp = NULL;

	if (select (max + 1, &rs, &ws, &es, tvp) == -1) {
		if (errno != EINTR)
			die ("Fatal error: select: %s\n", strerror (errno));
	}

	MHD_run (daemon);
}


extern void
init_suspend_pool (void)
{
	pool = vector_new ();

	if (pool == NULL)
		die ("failed to initialize suspend pool");
}


extern void
resume_all_connections (struct MHD_Daemon *daemon)
{
	static size_t total;
	static size_t i;
	static void *entry;
	static struct MHD_Connection *connection;
	(void) daemon;


	total = vector_count (pool);

	if (total == 0)
		return;

	debug ("* Resuming all %zu connections\n", total);

	for (i = 0; i < total; i++) {
		if (vector_get (pool, i, &entry)) {
			connection = (struct MHD_Connection *) entry;
			MHD_resume_connection (connection);
		}
		else
			die ("failed to get connection entry");
	}

	/* purge pool */
	if (! vector_reset (pool))
		die ("failed to purge pool");

/*
	run_daemon (daemon);
*/
}


extern void
resume_connection (size_t index, struct MHD_Daemon *daemon)
{
	static void *entry;
	static struct MHD_Connection *connection;
	(void) daemon;


	if (vector_get (pool, index, &entry)) {
		connection = (struct MHD_Connection *) entry;
#if defined(_DEBUG)
		warn (connection, "resume");
#endif
		MHD_resume_connection (connection);

		if (! vector_delete (pool, index, NULL))
			die ("failed to delete connection from pool");
/*
		run_daemon (daemon);
*/
	}
}


extern size_t
suspend_connection (struct MHD_Connection *connection)
{
	static size_t index;

	if (! vector_add (pool, (void *) connection))
		die ("failed to add connection to pool (%d)",
			vector_get_errno (pool));

	index = vector_count (pool) - 1;

	MHD_suspend_connection (connection);

#if defined(_DEBUG)
	warn (connection, "suspend");
#endif

	return index;
}

