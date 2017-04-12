#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vec.h"

#define VECSIZ 16

union vec {
	void const *p;
	char **v;
};

static int vec_expand(char **, size_t);

int
vec_alloc(void *vecp, size_t size)
{
	size_t siz = 0;
	union vec vec = {.p = vecp};

	assert(vec.v != 0x0);

	if (mulz_overflows(VECSIZ, size)) return EOVERFLOW;
	siz = sizeof (size_t) * 3 + VECSIZ * size;
	*vec.v = malloc(siz);
	if (!*vec.v) return ENOMEM;

	*vec.v = *vec.v + 3 * sizeof (size_t);

	len(*vec.v) = 0;
	siz(*vec.v) = size;
	mem(*vec.v) = VECSIZ * size;

	memset(*vec.v, 0, VECSIZ * size);

	return 0;
}

int
vec_append(void *destp, void const *data)
{
	union vec dest = {.p = destp};
	vec_check(dest.v);
	return vec_splice(destp, len(*dest.v), data, 1);
}

void *
vec_clone(void const *vecp)
{
	void * ret = 0x0;
	union vec vec = {.p = vecp};

	if (vec_alloc(&ret, siz(*vec.v)))
		return 0x0;

	if (vec_join(&ret, *vec.v)) {
		vec_free(ret);
		return 0;
	}

	return ret;
}

int
vec_copy(void *destp, void *src)
{
	union vec dest = {.p = destp};

	vec_check(dest.v);
	vec_check(&src);

	vec_truncate(destp, 0);

	return vec_splice(destp, 0, src, len(src));
}

int
vec_concat(void *destp, void const *src, size_t nmemb)
{
	union vec dest = {.p = destp};
	vec_check(dest.v);
	return vec_splice(destp, len(*dest.v), src, nmemb);
}

void
vec_delete(void *vecp, size_t which)
{
	vec_elim(vecp, which, 1);
}

void
vec_elim(void *vecp, size_t off, size_t ext)
{
	size_t min = 0;
	union vec vec = {.p = vecp};

	if (off > len(*vec.v)) return;

	min = minz(ext, len(*vec.v) - off);

	memmove(*vec.v + off * siz(*vec.v),
		*vec.v + (off + min) * siz(*vec.v),
		(len(*vec.v) - off - min) * siz(*vec.v));

	len(*vec.v) -= min;

	memset(*vec.v + len(*vec.v),
	       0,
	       min * siz(*vec.v));
}

int
vec_expand(char **vecv, size_t diff)
{
	char *tmp = 0x0;

	assert(mem(*vecv) != 0);

	tmp = realloc(*vecv - 3 * sizeof (size_t),
			diff + 3 * sizeof (size_t));
	if (!tmp) return ENOMEM;

	*vecv = tmp + 3 * sizeof (size_t);
	mem(*vecv) = diff;

	return 0;
}

int
vec_insert(void *vecp, void const * data, size_t pos)
{
	union vec vec = {.p = vecp};
	vec_check(vec.v);
	return vec_splice(vecp, pos, data, 1);
}

int
vec_join(void *destp, void const *src)
{
	union vec dest = {.p = destp};
	vec_check(dest.v);
	return vec_splice(destp, len(*dest.v), src, len(src));
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
	vec_check(vec.v);
	vec_slice(vecp, off, len(*vec.v) - off);
}

void
vec_slice(void *vecp, size_t beg, size_t ext)
{
	size_t min = 0;
	union vec vec = {.p = vecp};

	vec_check(vec.v);

	if (beg >= len(*vec.v)) {
		vec_truncate(vecp, 0);
		return;
	}

	min = minz(ext, len(*vec.v) - beg);

	memmove(*vec.v,
		*vec.v + beg * siz(*vec.v),
		min * siz(*vec.v));

	memset(*vec.v + (beg + min) * siz(*vec.v),
		0,
		(len(*vec.v) - min - beg) * siz(*vec.v));

	len(*vec.v) = min;
}

int
vec_splice(void *destp, size_t pos, void const *src, size_t nmemb)
{
	int err = 0;
	size_t ext = 0;
	size_t len = 0;
	size_t off = 0;
	size_t mem = 0;
	union vec dest = {.p = destp};

	if (off > len(*dest.v)) return EINVAL;

	if (addz_overflows(len(*dest.v), nmemb))
		return EOVERFLOW;

	ext = nmemb * siz(*dest.v);
	len = len(*dest.v) * siz(*dest.v);
	off = pos * siz(*dest.v);

	while (len + ext >= mem(*dest.v)) {
		mem = mem(*dest.v);
		while (mem <= len + ext) mem *= 2;
		err = vec_expand(dest.v, mem);
		if (err) return err;
	}

	memmove(*dest.v + off + ext,
		*dest.v + off,
		len - off);

	memcpy(*dest.v + off, src, ext);

	len(*dest.v) += nmemb;

	return 0;
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
	return vec_splice(destp, 0, data, len);
}
