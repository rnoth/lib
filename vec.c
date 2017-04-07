#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vec.h"

#define VECSIZ 16
#define GROWTH 2
#define HEADER (sizeof (size_t) * 2)

static int vec_expand(void **);

int
vec_alloc(void **vecp, size_t size)
{
	*vecp = malloc(sizeof (size_t) * 2 + VECSIZ * size);
	if (!*vecp) return ENOMEM;

	*vecp = (size_t *)*vecp + 2;

	len(*vecp) = 0;
	mem(*vecp) = VECSIZ;

	memset(*vecp, 0, VECSIZ * size);

	return 0;
}

int
_vec_append(void **vec, void const *data, size_t size)
{
	return _vec_insert(vec, data, len(*vec), size);
}

void *
_vec_clone(void const *vec, size_t size)
{
	void *ret = 0;

	if (vec_alloc(&ret, size)) return 0;

	if (_vec_join(&ret, vec, size)) {
		vec_free(ret);
		return 0;
	}

	return ret;
}

int
_vec_concat(void **vecp, void const *data, size_t nmemb, size_t size)
{
	char *arr =;
	size_t ext = nmemb * size;
	size_t len = len(*vecp) * size;

	if (len(*vecp) + nmemb < len(*vecp)) return EOVERFLOW;

	while (len + ext >= mem(*vecp))
		if (vec_expand(vecp)) return ENOMEM;

	memcpy((char *)*vecp + len(*vecp) * size, data, len);
	len(*vecp) += nmemb;

	return 0;
}

void
_vec_delete(void *vec, size_t which, size_t size)
{
	if (which > len(vec)) return;

	memmove((char *)vec + which * size, (char *)vec + (which + 1) * size, (len(vec) - which) * size);

	--len(vec);
}

int
vec_expand(void **vecp)
{
	char *tmp;

	assert(mem(*vecp));

	tmp = realloc((size_t *)*vecp - 2, mem(*vecp) * GROWTH);
	if (!tmp) return ENOMEM;

	*vecp = (size_t *)tmp + 2;
	mem(*vecp) *= GROWTH;

	return 0;
}

int
_vec_insert(void **vecp, void const *data, size_t pos, size_t size)
{
	char *arr = 0;
	size_t ext = 0;
	size_t len = 0;

	if (pos > len(*vecp)) return EINVAL;

	arr = *vecp;
	ext = pos * size;
	len = len(arr) * size;

	if (len + size >= mem(arr) && vec_expand(vecp)) return ENOMEM;

	arr = *vecp;

	memmove(arr + ext + size, arr + ext, len - ext);
	memcpy(arr + ext, data, size);

	++len(*vecp);

	return 0;
}

int
_vec_join(void **dest, void const *src, size_t size)
{
	return _vec_concat(dest, src, len(src), size);
}

void
vec_free(void *vec)
{
	if (!vec) return;

	free((char *)vec - HEADER);
}

void
_vec_shift(void *vec, size_t off, size_t size)
{
	_vec_slice(vec, off, len(vec) - off, size);
}

void
_vec_slice(void *vec, size_t beg, size_t ext, size_t size)
{
	size_t min;

	if (beg >= len(vec)) {
		_vec_truncate(vec, 0, size);
		return;
	}

	min = MIN(ext, len(vec) - beg);

	memmove(vec, (char *)vec + beg * size, min * size);
	memset((char *)vec + (beg + min) * size, 0, (len(vec) - min - beg) * size);

	len(vec) = min;
}

void
_vec_truncate(void *vec, size_t off, size_t size)
{
	memset((char *)vec + off * size, 0, (len(vec) - off) * size);
	len(vec) = off;
}
