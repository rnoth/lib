#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vec.h"

#define VECSIZ 8
#define HEADER (sizeof (size_t))

#undef vec_check
#define vec_check(vecv, size) do {                               \
	vec_assert(*vecv != 0x0);                                \
	vec_assert(vec_len(*vecv) <= vec_mem(*vecv, size));      \
	vec_assert(vec_mem(*vecv, size) > size);                 \
	vec_assert(vec_len(*vecv) * size <= vec_mem(*vecv, size));   \
} while (0)

#undef vec_mem

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
	unsigned char **v;
};

static int vec_expand(unsigned char **, size_t, size_t);


size_t
vec_mem(void const *v, size_t size)
{
	size_t off = len(v) * size;
	union {
		void const *v;
		unsigned char const *y;
	} vec = { .v = v };

	while (vec.y[off] != 0xff) ++off;

	return -~off;
}


int
vec_alloc(void *vecp, size_t size)
{
	size_t siz = 0;
	union vec vec = {.p = vecp};

	assert(vec.v != 0x0);

	if (mulz_overflows(VECSIZ, size)) return EOVERFLOW;
	siz = VECSIZ * size;

	if (addz_overflows(HEADER + 1, siz)) return EOVERFLOW;
	siz = HEADER + 1 + siz;

	*vec.v = calloc(1, siz);
	if (!*vec.v) return ENOMEM;

	*vec.v = *vec.v + HEADER;

	len(*vec.v) = 0;
	(*vec.v)[VECSIZ * size - 1] = 0xff;

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

	if (ind > vec_len(*vec.v)) return;

	len = vec_len(*vec.v) * size;
	off = ind * size;
	ext = umin(nmemb * size, len - off);

	memmove(*vec.v + off,
	        *vec.v + off + ext,
	        len - off - ext);

	len(*vec.v) -= umin(nmemb, len(*vec.v));

	memset(*vec.v + vec_len(*vec.v) * size, 0, ext);
}

int
vec_expand(unsigned char **vecv, size_t size, size_t diff)
{
	unsigned char *old = 0x0;
	unsigned char *tmp = 0x0;

	assert(vec_mem(*vecv, size) != 0);

	old = *vecv - HEADER;

	(*vecv)[vec_mem(*vecv, size) - 1] = 0;

	tmp = calloc(1, diff + HEADER);
	if (!tmp) return ENOMEM;
	memcpy(tmp, old, len(*vecv) * size + HEADER);
	free(old);

	*vecv = tmp + HEADER;

	(*vecv)[diff - 1] = 0xff;
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
vec_slice(void *vecp, size_t beg, size_t nmemb, size_t size)
{
	size_t ext = 0;
	size_t off = 0;
	size_t len = 0;
	size_t min = 0;
	union vec vec = {.p = vecp};

	vec_check(vec.v, size);

	if (beg >= len(*vec.v)) {
		vec_truncate(vecp, 0, size);
		return;
	}

	min = umin(nmemb, len(*vec.v) - beg);

	ext = min * size;
	off = beg * size;
	len = len(*vec.v) * size;


	memmove(*vec.v, *vec.v + off, ext);
	memset(*vec.v + ext, 0, len - ext);

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

	while (len + ext >= vec_mem(*dest.v, size)) {
		mem = vec_mem(*dest.v, size);
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
