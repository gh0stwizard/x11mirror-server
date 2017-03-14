#ifndef _SIMPLE_MUTEX_H
#define _SIMPLE_MUTEX_H

#if defined(_WIN32)
#include <windows.h>
#else
#include <stdbool.h>
#include <pthread.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct _SIMPLE_MUTEX SIMPLE_MUTEX;


extern SIMPLE_MUTEX *
simple_mutex_create (void);

extern void
simple_mutex_init (SIMPLE_MUTEX *mutex);

extern void
simple_mutex_destroy (SIMPLE_MUTEX *mutex);


extern inline void simple_mutex_lock(SIMPLE_MUTEX *mutex);
extern inline void simple_mutex_unlock(SIMPLE_MUTEX *mutex);

#if defined(_WIN32)

struct _SIMPLE_MUTEX {
	PCRITICAL_SECTION cs;
};

extern inline void
simple_mutex_lock (SIMPLE_MUTEX *mutex)
{
	if (mutex->cs)
		EnterCriticalSection (mutex->cs);
}


extern inline void
simple_mutex_unlock (SIMPLE_MUTEX *mutex)
{
	if (mutex->cs)
		LeaveCriticalSection (mutex->cs);
}

#else

struct _SIMPLE_MUTEX {
	bool inited;
	pthread_mutex_t mutex;
};


extern inline void
simple_mutex_lock(SIMPLE_MUTEX *mutex)
{
	if (mutex->inited)
		pthread_mutex_lock (&mutex->mutex);
}


extern inline void
simple_mutex_unlock(SIMPLE_MUTEX *mutex)
{
	if (mutex->inited)
		pthread_mutex_unlock (&mutex->mutex);
}

#endif

#ifdef __cplusplus
	}
#endif

#endif /* _SIMPLE_MUTEX_H */
