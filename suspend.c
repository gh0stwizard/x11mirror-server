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


/* ------------------------------------------------------------------ */


extern void
init_suspend_pool (void)
{
	pool = vector_new ();

	if (pool == NULL)
		die ("failed to initialize suspend pool");
}


extern void
free_suspend_pool (void)
{
	if (pool != NULL) {
		/* don't free entries, just set to NULL */
		vector_reset (pool);
		/* completely destroy the pool */
		vector_destroy (pool);
		pool = NULL;
	}
}


extern void
resume_all_connections (void)
{
	size_t total, i;
	void *entry;	
	struct MHD_Connection *connection;


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
			MHD_resume_connection (connection);
#if defined(_DEBUG)
			warn (connection, "resumed");
#endif
		}
		else
			/* XXX: thread-safe die() */
			debug ("! FATAL ERROR: get connection entry");
	}

	/* purge the pool */
	if (vector_count (pool) == total) {
		if (! vector_reset (pool))
			/* XXX: thread-safe die() */
			debug ("! FATAL ERROR: reset pool");
	}
}


extern void
resume_next (void)
{
	size_t total;
	void *entry;	
	struct MHD_Connection *connection;


	total = vector_count (pool);

	if (total == 0)
		return;

	if (! vector_delete (pool, 0, &entry)) {
		/* XXX: thread-safe die() */
		debug ("! FATAL ERROR: resume next connection: "
			"vector_delete: %s\n",
			strerror (vector_get_errno (pool)));
		return;
	}

	if (entry != NULL) {
		connection = (struct MHD_Connection *)entry;
		MHD_resume_connection (connection);
#if defined(_DEBUG)
		warn (connection, "resumed");
#endif
	}
	else
		debug ("! ERROR: resume next connection: invalid entry\n");
}


extern void
suspend_connection (struct MHD_Connection *connection)
{
	if (vector_add (pool, (void *) connection)) {
		MHD_suspend_connection (connection);
#if defined(_DEBUG)
		warn (connection, "suspend");
#endif
	}
	else {
		/* XXX: thread-safe die() */
		debug ("! FATAL ERROR: add connection to pool: %s\n",
			strerror (vector_get_errno (pool)));
	}
}
