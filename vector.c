/* Originally written by Emil Hernvall
 *     https://gist.github.com/953968
 * Peter Hornyack and Katelin Bailey, 12/8/11, University of Washington
 *     https://gist.github.com/pjh/1453219
 */
#include "vector.h"
#ifndef _NO_THREADS
#include "mutex.h"
#endif

#include <stdio.h> /* DEBUG */
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

#if defined(_WIN32)
#include <search.h>	/* qsort_s */
#endif

#define VECTOR_INIT_SIZE	2
#define VECTOR_RESIZE_FACTOR	2
#define VECTOR_MAX ((UINT_MAX / (sizeof(void *) * VECTOR_RESIZE_FACTOR)) - 1)


#ifdef __cplusplus
extern "C" {
#endif

struct _afv_vector {
	void 		**data;
	size_t 		count;
	size_t 		size;
	int		errnum;
#ifndef _NO_THREADS
	SIMPLE_MUTEX	*mutex;
#endif
};


void
vector_set_errno(VECTOR *l, int errnum)
{
	l->errnum = errnum;
}


int
vector_get_errno(VECTOR *l)
{
	return l->errnum;
}


VECTOR *
vector_new(void)
{
	VECTOR *l = malloc(sizeof(*l));

	if (l != NULL) {
		l->count = 0;
		l->size = VECTOR_INIT_SIZE;
		l->data = malloc(sizeof(void *) * l->size);
		l->errnum = 0;

		if (l->data == NULL) {
			vector_set_errno(l, errno);
			free(l);
			l = NULL;
		}

#ifndef _NO_THREADS
		l->mutex = simple_mutex_create ();
		simple_mutex_init (l->mutex);
#endif
	}

	return l;
}


void
vector_destroy(VECTOR *l)
{
	if (l != NULL) {
#ifndef _NO_THREADS
		simple_mutex_lock (l->mutex);
#endif

		if (l->data != NULL)
			free(l->data);
		free(l);

#ifndef _NO_THREADS
		simple_mutex_unlock (l->mutex);
#endif
	}
}


bool
vector_add(VECTOR *l, void *e)
{
	void **temp;
	size_t new_size;


	assert (l != NULL);

	if (l->size < VECTOR_MAX) {
#ifndef _NO_THREADS
		simple_mutex_lock (l->mutex);
#endif
		/* if the current allocated size of vector structure
		 * is exceeding the value below, we throw an error.
		 */
		if (l->size == l->count) {
			new_size = l->size * VECTOR_RESIZE_FACTOR;
			temp = realloc(l->data, sizeof(void *) * new_size);

			if (temp != NULL) {
				l->data = temp;
				l->size = new_size;
			}
			else {
				vector_set_errno(l, errno);
				return false;
			}
		}

		l->data[l->count] = e;
		l->count++;

#ifndef _NO_THREADS
		simple_mutex_unlock (l->mutex);
#endif
		return true;
	}
	else {
		/* overflow */
		vector_set_errno(l, ERANGE);
		return false;
	}
}


bool
vector_get(VECTOR *l, size_t index, void **e)
{
	assert(l != NULL);


	if (e == NULL) {
		vector_set_errno(l, EINVAL);
		return false;
	}

	if (index >= l->count) {
		vector_set_errno(l, ERANGE);
		return false;
	}

	*e = l->data[index];

	return true;
}


bool
vector_shrink(VECTOR *l)
{
	void **temp;
	size_t new_size;


#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	if (	(l->size > VECTOR_INIT_SIZE) &&
		(l->count <= l->size / (VECTOR_RESIZE_FACTOR * 2)))
	{
		new_size = l->size / VECTOR_RESIZE_FACTOR;
		temp = realloc(l->data, sizeof(void *) * l->size);

		if (temp != NULL) {
			l->data = temp;
			l->size = new_size;
		}
		else {
#ifndef _NO_THREADS
			simple_mutex_unlock (l->mutex);
#endif
			vector_set_errno(l, errno);
			return false;
		}
	}

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif

	return true;
}


bool
vector_delete(VECTOR *l, size_t index, void **e)
{
	size_t i, t;

	
	assert (l != NULL);

	if (index >= l->count) {
		vector_set_errno(l, ERANGE);
		return false;
	}

	if (e != NULL)
		*e = l->data[index];

#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif
	for (i = index, t = l->count - 1; i < t; i++)
		l->data[i] = l->data[i + 1];

	l->count--;

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif

	return vector_shrink(l);
}


void
vector_free_contents(VECTOR *l)
{
	size_t i, count;


#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	for (i = 0, count = l->count; i < count; i++)
		if (l->data[i] != NULL)
			free(l->data[i]);

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif
}


bool
vector_reset(VECTOR *l)
{
	bool result;


#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	for (size_t i = 0, max = l->count; i < max; i++)
		l->data[i] = NULL;

	void **temp = realloc(l->data, sizeof(void *) * VECTOR_INIT_SIZE);

	if (temp != NULL) {
		l->data = temp;
		l->count = 0;
		l->size = VECTOR_INIT_SIZE;
		result =  true;
	}
	else {
		vector_set_errno(l, errno);
		result = false;
	}

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif

	return result;
}


bool
vector_set(VECTOR *l, size_t index, void *e, void **old)
{
	bool result;

	assert (l != NULL);

#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	if (index >= l->count) {
		vector_set_errno(l, ERANGE);
		result = false;
	}
	else {
		if (old != NULL)
			*old = l->data[index];

		l->data[index] = e;
		result = true;
	}

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif

	return result;
}


size_t
vector_count(VECTOR *l)
{
	if (l != NULL)
		return l->count;
	else
		return UINT_MAX;
}


void
vector_qsort(VECTOR *l, int(*cmp)(const void *, const void *))
{
#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	qsort((void *)l->data, l->count, sizeof(void *), cmp);

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif
}


#if defined(_WIN32)
void
vector_qsort_s(VECTOR *l,
	int(*cmp)(void *, const void *, const void *), void * ctx)
{
#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	qsort_s((void *)l->data, l->count, sizeof(void *), cmp, ctx);

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif
}
#endif


#if defined(_GNU_SOURCE)
void
vector_qsort_r(VECTOR *l,
	int(*cmp)(const void *, const void *, void *), void * ctx)
{
#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	qsort_r((void *)l->data, l->count, sizeof(void *), cmp, ctx);

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif
}
#endif

#ifdef __cplusplus
}
#endif
