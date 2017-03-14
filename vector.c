/* Originally written by Emil Hernvall
 *     https://gist.github.com/953968
 * Peter Hornyack and Katelin Bailey, 12/8/11, University of Washington
 *     https://gist.github.com/pjh/1453219
 */
#include "vector.h"
#ifndef _NO_THREADS
#include "mutex.h"
#endif

#include <stdlib.h>
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
#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	if (l != NULL) {
		if (l->data != NULL)
			free(l->data);

		free(l);
	}

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif
}


bool
vector_add(VECTOR *l, void *e)
{
	void **temp;
	bool result;

#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	if (l == NULL) {
		vector_set_errno(l, EINVAL);
		result = false;
	}
	else if (l->size >= VECTOR_MAX) {
		/* if the current allocated size of vector structure
		 * is exceeding the value below, we throw an error.
		 */
		vector_set_errno(l, ERANGE);
		result = false;
	}
	else if (l->size == l->count) {
		l->size *= VECTOR_RESIZE_FACTOR;

		temp = realloc(l->data, sizeof(void *) * l->size);

		if (temp != NULL) {
			l->data = temp;
			result = true;
		}
		else {
			vector_set_errno(l, errno);
			result = false;
		}
	}
	else
		result = true;

	if (result) {
		l->data[l->count] = e;
		l->count++;
	}

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif

	return result;
}


bool
vector_get(VECTOR *l, size_t index, void **e)
{
	bool result;

#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	if ((l == NULL) || (e == NULL)) {
		vector_set_errno(l, EINVAL);
		result = false;
	}
	else if (index >= l->count) {
		vector_set_errno(l, ERANGE);
		result = false;
	}
	else {
		*e = l->data[index];
		result = true;
	}

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif
	return result;
}


bool
vector_shrink(VECTOR *l)
{
	void **temp;
	bool result;

#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	if ((l->size > VECTOR_INIT_SIZE)
		&& (l->count <= l->size / (VECTOR_RESIZE_FACTOR * 2)))
	{
		l->size /= VECTOR_RESIZE_FACTOR;
		temp = realloc(l->data, sizeof(void *) * l->size);
		if (temp != NULL) {
			l->data = temp;
			result = true;
		}
		else {
			vector_set_errno(l, errno);
			result = false;
		}
	}
	else
		result = true;

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif

	return result;
}


bool
vector_delete(VECTOR *l, size_t index, void **e)
{
	size_t i, t;
	bool result;

#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	if (l == NULL) {
		vector_set_errno(l, EINVAL);
		result = false;
	}
	else if (index >= l->count) {
		vector_set_errno(l, ERANGE);
		result = false;
	}
	else {
		if (e != NULL)
			*e = l->data[index];

		for (i = index, t = l->count - 1; i < t; i++)
			l->data[i] = l->data[i + 1];

		l->count--;
		result = vector_shrink(l);
	}

#ifndef _NO_THREADS
	simple_mutex_unlock (l->mutex);
#endif

	return result;
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

#ifndef _NO_THREADS
	simple_mutex_lock (l->mutex);
#endif

	if (l == NULL) {
		vector_set_errno(l, EINVAL);
		result = false;
	}
	else if (index >= l->count) {
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
