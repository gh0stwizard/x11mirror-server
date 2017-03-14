/* Originally written by Emil Hernvall
 *     https://gist.github.com/953968
 * Peter Hornyack and Katelin Bailey, 12/8/11, University of Washington
 *     https://gist.github.com/pjh/1453219
 */
#ifndef _AFV_VECTOR_H__
#define _AFV_VECTOR_H__

#include <stdbool.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _afv_vector VECTOR;

/* Return an error code */
int vector_get_errno(VECTOR *l);

/* Set the error number */
void vector_set_errno(VECTOR *l, int errnum);

/* Allocates and initializes a vector. The vector should later be freed
 * by passing it to vector_destroy().
 * Returns a pointer to vector on success or NULL, you may retrieve
 * error code by vector_get_errno().
 */
VECTOR * vector_new(void);

/* Frees the vector's array and the vector struct itself. NOTE: if the
 * array contains pointers to other data, the data that is pointed to
 * is NOT freed!
 */
void vector_destroy(VECTOR *l);

/* Calls free() on all non-null pointers that are stored in the vector.
 * It does not remove these pointers from the vector however, so the
 * vector's element count will be unchanged.
 */
void vector_free_contents(VECTOR *l);

/* Shrinks the vector by VECTOR_RESIZE_FACTOR.
 * Returns: true on success, false on error. To get an error code use
 * vector_get_errno().
 */
bool vector_shrink(VECTOR *l);

/* Shrinks the vector to its initial size.
 * Returns: true on success, false on error. To get an error code use
 * vector_get_errno().
 */
bool vector_reset(VECTOR *l);

/* Appends an element to the vector.
 * Returns: true on success, false on error. To get an error code use
 * vector_get_errno().
 */
bool vector_add(VECTOR *l, void *e);

/* Gets the element at the specified index.
 * Returns: true on success, false on error. On success, *e is set to point to
 * the gotten element. To get an error code use vector_get_errno().
 */
bool vector_get(VECTOR *l, size_t index, void **e);

/* Removes the element at the specified index, and shifts all of the
 * remaining elements down in the vector. Importantly, the element
 * itself is NOT freed; if e is non-NULL, then *e is set to point to
 * the element, so that the caller can free it.
 * Returns: true on success, false on error. To get an error code 
 * use vector_get_errno().
 */
bool vector_delete(VECTOR *l, size_t index, void **e);

/* Replaces the element at the specified index.
 * Returns: true on success, false on error. On success, *old is set to point
 * to the element that was previously stored in the slot. To get an error code
 * use vector_get_errno().
 */
bool vector_set(VECTOR *l, size_t index, void *e, void **old);

/* Returns total number of the elements. */
size_t vector_count(VECTOR *l);

/* Calls qsort() function provided by the C standard library. */
void vector_qsort(VECTOR *l, int(*cmp)(const void *, const void *));

#if defined(_WIN32) || defined(_WIN64)
/* Calls qsort_s() function on Windows. */
void
vector_qsort_s(VECTOR *l,
	int(*cmp)(void *, const void *, const void *), void * ctx);
#endif

#if defined(_GNU_SOURCE)
/* Calls qsort_r() function (GNU variant). */
void
vector_qsort_r(VECTOR *l,
	int(*cmp)(const void *, const void *, void *), void * ctx);
#endif

#ifdef __cplusplus
}
#endif
#endif /* _AFV_VECTOR_H__ */
