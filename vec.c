#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vec.h"

#define VECSIZ 16

#define sanitycheck(vecv) do {			\
	assert(vecv != 0x0);			\
	assert(*vecv != 0x0);			\
	assert(*vecv != (void*)(sizeof (size_t) * 3));	\
	assert(len(*vecv) <= mem(*vecv));	\
	assert(siz(*vecv) < mem(*vecv));	\
	assert(len(*vecv) * siz(*vecv) <= mem(*vecv)); \
} while (0)

union vec {
	void const *p;
	char **v;
};

static int vec_expand(char **);

int
vec_alloc(void *vecp, size_t size)
{
	size_t siz = 0;
	union vec vec = {.p = vecp};

	assert(vec.v != 0x0);

	siz = sizeof (size_t) * 3 + VECSIZ * size;
	*vec.v = malloc(siz);
	if (!*vec.v) return ENOMEM;

	*vec.v = *vec.v + 3 * sizeof (size_t);

	len(*vec.v) = 0;
	siz(*vec.v) = size;
	mem(*vec.v) = VECSIZ;

	memset(*vec.v, 0, VECSIZ * size);

	return 0;
}

int
vec_append(void *vecp, void const *data)
{
	union vec vec = {.p = vecp};
	sanitycheck(vec.v);
	return vec_insert(vecp, data, len(*vec.v));
}

void *
vec_clone(void const *vecp)
{
	void * ret = 0x0;
	union vec vec = {.p = vecp};

	if (vec_alloc(&ret, siz(*vec.v)))
		return 0x0;

	if (vec_join(&ret, vec.v)) {
		vec_free(ret);
		return 0;
	}

	return ret;
}

int
vec_copy(void *destp, void *srcp)
{
	union vec dest = {.p = destp};
	union vec src = {.p = srcp};

	sanitycheck(dest.v);
	sanitycheck(src.v);

	vec_truncate(destp, 0);
	return vec_join(destp, srcp);
}

int
vec_concat(void *vecp, void const * data, size_t nmemb)
{
	size_t ext = 0;
	size_t len = 0;
	union vec vec = {.p = vecp};

	sanitycheck(vec.v);

	ext = nmemb * siz(*vec.v);
	len = len(*vec.v) * siz(*vec.v);

	if (len(*vec.v) + nmemb < len(*vec.v))
		return EOVERFLOW;

	while (len + ext >= mem(*vec.v))
		if (vec_expand(vec.v))
			return ENOMEM;

	memcpy(*vec.v + len, data, ext);
	len(*vec.v) += nmemb;

	return 0;
}

void
vec_delete(void *vecp, size_t which)
{
	union vec vec = {.p = vecp};

	sanitycheck(vec.v);

	if (which > len(*vec.v)) return;

	memmove(*vec.v + which * siz(*vec.v),
		*vec.v + (which + 1) * siz(*vec.v),
		(len(*vec.v) - which) * siz(*vec.v));

	--len(*vec.v);
}

int
vec_expand(char **vecv)
{
	char *tmp;
	size_t newsiz;

	assert(mem(*vecv) != 0);

	newsiz = mem(*vecv) * 2 + sizeof (size_t) * 3;
	tmp = realloc((size_t *)*vecv - 3, newsiz);
	if (!tmp) return ENOMEM;

	*vecv = (char *)tmp + 3 * sizeof (size_t);
	mem(*vecv) *= 2;

	return 0;
}

int
vec_insert(void *vecp, void const * data, size_t pos)
{
	char *arr = 0;
	int err = 0;
	size_t ext = 0;
	size_t len = 0;
	union vec vec = {.p = vecp};

	sanitycheck(vec.v);

	if (pos > len(*vec.v)) return EINVAL;

	arr = *vec.v;
	ext = pos * siz(*vec.v);
	len = len(arr) * siz(*vec.v);

	if (len + siz(*vec.v) >= mem(arr)) {
		err = vec_expand(vecp);
		if (err) return err;
	}

	arr = *vec.v;

	memmove(arr + ext + siz(*vec.v), arr + ext, len - ext);
	memcpy(arr + ext, data, siz(arr));

	++len(*vec.v);

	return 0;
}

int
vec_join(void * destp, void const * srcp)
{
	union vec src = {.p = srcp};
	sanitycheck(src.v);
	return vec_concat(destp, *src.v, len(*src.v));
}

void
vec_free(void *vec)
{
	if (!vec) return;
	free((char *)vec - sizeof (size_t) * 3);
}

void
vec_shift(void *vecp, size_t off)
{
	union vec vec = {.p = vecp};
	sanitycheck(vec.v);
	vec_slice(vecp, off, len(*vec.v) - off);
}

void
vec_slice(void *vecp, size_t beg, size_t ext)
{
	size_t min = 0;
	union vec vec = {.p = vecp};

	sanitycheck(vec.v);

	if (beg >= len(*vec.v)) {
		vec_truncate(vecp, 0);
		return;
	}

	min = umin(ext, len(*vec.v) - beg);

	memmove(*vec.v,
		*vec.v + beg * siz(*vec.v),
		min * siz(*vec.v));
	memset(*vec.v + (beg + min) * siz(*vec.v),
		0,
		(len(*vec.v) - min - beg) * siz(*vec.v));

	len(*vec.v) = min;
}

void
vec_truncate(void *vecp, size_t off)
{
	union vec vec = {.p = vecp};
	memset(*vec.v + off * siz(*vec.v),
		0,
		(len(*vec.v) - off) * siz(*vec.v));
	len(*vec.v) = off;
}

int
vec_transfer(void *destp, void const *data, size_t len)
{
	vec_truncate(destp, 0);
	return vec_concat(destp, data, len);
}
