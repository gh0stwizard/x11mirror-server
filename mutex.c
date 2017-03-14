#include "mutex.h"
#include <assert.h>
#include <stdlib.h>


#if defined(_WIN32)
extern void
simple_mutex_init (SIMPLE_MUTEX *mutex)
{
	assert (mutex);

	if (! mutex->cs)
		mutex->cs = malloc (sizeof (*mutex->cs));

	assert (mutex->cs);

	if (mutex->cs)
		InitializeCriticalSection (mutex->cs);
	else
		abort ();
}


extern void
simple_mutex_destroy (SIMPLE_MUTEX *mutex)
{
	assert (mutex);

	if (mutex->cs) {
		DeleteCriticalSection (mutex->cs);
		free (mutex->cs);
		mutex->cs = NULL;
	}
}


#else /* GNU/Linux and rest Unixes */

extern void
simple_mutex_init (SIMPLE_MUTEX *mutex)
{
	assert (mutex);

	pthread_mutex_init (&mutex->mutex, NULL);
	mutex->inited = true;
}


extern void
simple_mutex_destroy (SIMPLE_MUTEX *mutex)
{
	assert (mutex);

	if (mutex->inited) {
		pthread_mutex_destroy (&mutex->mutex);
		mutex->inited = false;
	}
}
#endif


extern SIMPLE_MUTEX *
simple_mutex_create (void)
{
	SIMPLE_MUTEX *mutex = malloc (sizeof *mutex);
	assert (mutex);
	return mutex;
}
