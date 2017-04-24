#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vec.h"

#define VECSIZ 8
#define HEADER (2 * sizeof (size_t))

#undef vec_check
#define vec_check(vecv, size) do {                     \
	vec_assert(*vecv != 0x0);                      \
	vec_assert(len(*vecv) <= mem(*vecv));          \
	vec_assert(size < mem(*vecv));                 \
	vec_assert(len(*vecv) * size <= mem(*vecv));   \
} while (0)


#undef vec_append
#undef vec_clone
#undef vec_concat
#undef vec_copy
#undef vec_delete
#undef vec_elim
#undef vec_insert
#undef vec_join
#undef vec_shift
#undef vec_slice
#undef vec_splice
#undef vec_transfer
#undef vec_truncate

union vec {
	void const *p;
	char **v;
};

static int vec_expand(char **, size_t, size_t);
static size_t vec_mem(char *);

#if 0
size_t
vec_mem(char *vec, size_t size)
{
	return rawmemchr(vec + len(vec) * size, 0xff) - vec;
}
#endif

int
vec_alloc(void *vecp, size_t size)
{
	size_t siz = 0;
	union vec vec = {.p = vecp};

	assert(vec.v != 0x0);

	if (mulz_overflows(VECSIZ, size)) return EOVERFLOW;
	siz = VECSIZ * size;

	if (addz_overflows(HEADER, siz)) return EOVERFLOW;
	siz = HEADER + siz;

	*vec.v = calloc(1, siz);
	if (!*vec.v) return ENOMEM;

	*vec.v = *vec.v + HEADER;

	len(*vec.v) = 0;
	mem(*vec.v) = VECSIZ * size;

	return 0;
}

int
vec_append(void *destp, void const *data, size_t size)
{
	union vec dest = {.p = destp};
	vec_check(dest.v, size);
	return vec_splice(destp, len(*dest.v), data, 1, size);
}

void *
vec_clone(void const *vec, size_t size)
{
	void *ret = 0x0;

	vec_check(&vec, size);

	if (vec_alloc(&ret, size))
		return 0x0;

	if (vec_join(&ret, vec, size)) {
		vec_free(ret);
		return 0;
	}

	return ret;
}

int
vec_copy(void *destp, void *src, size_t size)
{
	union vec dest = {.p = destp};

	vec_check(dest.v, size);
	vec_check(&src, size);

	vec_truncate(destp, 0, size);

	return vec_splice(destp, 0, src, len(src), size);
}

int
vec_concat(void *destp, void const *src, size_t nmemb, size_t size)
{
	union vec dest = {.p = destp};
	vec_check(dest.v, size);
	return vec_splice(destp, len(*dest.v), src, nmemb, size);
}

void
vec_delete(void *vecp, size_t which, size_t size)
{
	vec_elim(vecp, which, 1, size);
}

void
vec_elim(void *vecp, size_t ind, size_t nmemb, size_t size)
{
	size_t len = 0;
	size_t ext = 0;
	size_t off = 0;
	union vec vec = {.p = vecp};

	if (ind > len(*vec.v)) return;

	ext = umin(nmemb, len(*vec.v) - ind) * size;
	len = len(*vec.v) * size;
	off = ind * size;

	memmove(*vec.v + off,
		*vec.v + off + ext,
		len - off - ext);

	len(*vec.v) -= nmemb;

	memset(*vec.v + len,
	       0,
	       ext);
}

int
vec_expand(char **vecv, size_t size, size_t diff)
{
	char *old = 0x0;
	char *tmp = 0x0;

	assert(mem(*vecv) != 0);

	old = *vecv - HEADER;

	tmp = calloc(1, diff + HEADER);
	if (!tmp) return ENOMEM;
	memcpy(tmp, old, len(*vecv) * size + HEADER);
	free(old);

	*vecv = tmp + HEADER;

	mem(*vecv) = diff;

	return 0;
}

int
vec_insert(void *vecp, void const * data, size_t pos, size_t size)
{
	union vec vec = {.p = vecp};
	vec_check(vec.v, size);
	return vec_splice(vecp, pos, data, 1, size);
}

int
vec_join(void *destp, void const *src, size_t size)
{
	union vec dest = {.p = destp};
	vec_check(dest.v, size);
	return vec_splice(destp, len(*dest.v), src, len(src), size);
}

void
vec_free(void *vec)
{
	if (!vec) return;
	free((char *)vec - HEADER);
}

void
vec_shift(void *vecp, size_t off, size_t size)
{
	union vec vec = {.p = vecp};
	vec_check(vec.v, size);
	vec_slice(vecp, off, len(*vec.v) - off, size);
}

void
vec_slice(void *vecp, size_t beg, size_t ext, size_t size)
{
	size_t min = 0;
	union vec vec = {.p = vecp};

	vec_check(vec.v, size);

	if (beg >= len(*vec.v)) {
		vec_truncate(vecp, 0, size);
		return;
	}

	min = umin(ext, len(*vec.v) - beg);

	memmove(*vec.v,
		*vec.v + beg * size,
		min * size);

	memset(*vec.v + (beg + min) * size,
		0,
		(len(*vec.v) - min - beg) * size);

	len(*vec.v) = min;
}

int
vec_splice(void *destp, size_t pos, void const *src, size_t nmemb, size_t size)
{
	int err = 0;
	size_t ext = 0;
	size_t len = 0;
	size_t off = 0;
	size_t mem = 0;
	union vec dest = {.p = destp};

	vec_check(dest.v, size);

	if (off > len(*dest.v)) return EINVAL;

	if (addz_overflows(len(*dest.v), nmemb))
		return EOVERFLOW;

	ext = nmemb * size;
	len = len(*dest.v) * size;
	off = pos * size;

	while (len + ext >= mem(*dest.v)) {
		mem = mem(*dest.v);
		while (mem <= len + ext) mem *= 2;
		err = vec_expand(dest.v, size, mem);
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
vec_truncate(void *vecp, size_t off, size_t size)
{
	union vec vec = {.p = vecp};
	memset(*vec.v + off * size,
		0,
		(len(*vec.v) - off) * size);
	len(*vec.v) = off;
}

int
vec_transfer(void *destp, void const *data, size_t len, size_t size)
{
	vec_truncate(destp, 0, size);
	return vec_splice(destp, 0, data, len, size);
}
