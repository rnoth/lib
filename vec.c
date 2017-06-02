#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vec.h"

#undef vec_mem
#undef vec_new

#undef vec_alloc
#undef vec_append
#undef vec_clone
#undef vec_concat
#undef vec_ctor
#undef vec_copy
#undef vec_delete
#undef vec_elim
#undef vec_insert
#undef vec_join
#undef vec_resize
#undef vec_pop
#undef vec_shift
#undef vec_slice
#undef vec_splice
#undef vec_transfer
#undef vec_truncat

size_t const vec_header_size = sizeof (size_t) * 2;

union vec {
	size_t        **z;
	void const     *p;
	unsigned char **v;
};

#define vec(p) { .p = p }

void *
vec_alloc(size_t size, size_t len)
{
	size_t mem;
	size_t *ret;

	mem = size * len + vec_header_size;

	ret = calloc(1, mem);
	if (!ret) return 0x0;

	ret[0] = size * len;

	return ret + 2;
}

int
vec_append(void *destp, void const *data, size_t size)
{
	union vec dest = {.p = destp};
	return vec_splice(destp, vec_len(*dest.v), data, 1, size);
}

void *
vec_clone(void const *vec, size_t size)
{
	void *ret = 0x0;

	ret = vec_alloc(size, umax(vec_len(vec), 8));

	if (!ret) return 0;

	if (vec_join(&ret, vec, size)) {
		vec_free(ret);
		return 0;
	}

	return ret;
}

int
vec_concat(void *destp, void const *src, size_t nmemb, size_t size)
{
	union vec dest = {.p = destp};
	return vec_splice(destp, vec_len(*dest.v), src, nmemb, size);
}

int
vec_copy(void *destp, void *src, size_t size)
{
	vec_truncat(destp, 0, size);
	return vec_splice(destp, 0, src, vec_len(src), size);
}

int
vec_ctor(void *vecp, size_t size)
{
	union vec vec = {.p = vecp};
	void *res = vec_alloc(size, 8);

	if (!res) return ENOMEM;

	*vec.v = res;
	return 0;
}

void
vec_delete(void *vecp, size_t which, size_t size)
{
	vec_elim(vecp, which, 1, size);
}

void
vec_elim(void *vecp, size_t ind, size_t nmemb, size_t size)
{
	union vec vec = {.p = vecp};
	size_t len = 0;
	size_t ext = 0;
	size_t off = 0;

	if (ind > vec_len(*vec.v)) return;

	len = vec_len(*vec.v) * size;
	off = ind * size;
	ext = umin(nmemb * size, len - off);

	memmove(*vec.v + off,
	        *vec.v + off + ext,
	        len - off - ext);

	(*vec.z)[-1] -= umin(nmemb, vec_len(*vec.v));

	memset(*vec.v + vec_len(*vec.v) * size, 0, ext);
}

int
vec_insert(void *vecp, void const * data, size_t pos, size_t size)
{
	return vec_splice(vecp, pos, data, 1, size);
}

int
vec_join(void *destp, void const *src, size_t size)
{
	union vec dest = {.p = destp};
	return vec_splice(destp, vec_len(*dest.v), src, vec_len(src), size);
}

void
vec_free(void *vec)
{
	if (!vec) return;
	free((char *)vec - vec_header_size);
}

void *
vec_new(size_t size)
{
	void *ret = 0;

	vec_ctor(&ret, size);

	return ret;
}

void
vec_pop(void *dest, void *srcp, size_t size)
{
	union vec src = {.p = srcp};
	size_t ext = 0;

	ext = (vec_len(*src.v) - 1) * size;
	
	memcpy(dest, *src.v + ext, size);

	vec_delete(src.v, vec_len(*src.v) - 1, size);
}

int
vec_resize(void *vecp, size_t new, size_t size)
{
	union vec vec = {.p = vecp};
	unsigned char *old = 0x0;
	unsigned char *tmp = 0x0;
	size_t ext = 0;

	old = *vec.v - vec_header_size;

	tmp = calloc(1, new + vec_header_size);
	if (!tmp) return ENOMEM;

	ext = vec_len(*vec.v) * size + vec_header_size;

	memcpy(tmp, old, ext);
	free(old);

	*vec.v = tmp + vec_header_size;

	(*vec.z)[-2] = new;
	return 0;
}

void
vec_shift(void *vecp, size_t off, size_t size)
{
	union vec vec = {.p = vecp};
	vec_slice(vecp, off, vec_len(*vec.v) - off, size);
}

void
vec_slice(void *vecp, size_t beg, size_t nmemb, size_t size)
{
	union vec vec = {.p = vecp};
	size_t ext = 0;
	size_t off = 0;
	size_t len = 0;
	size_t min = 0;

	if (beg >= vec_len(*vec.v)) {
		vec_truncat(vecp, 0, size);
		return;
	}

	min = umin(nmemb, vec_len(*vec.v) - beg);

	ext = min * size;
	off = beg * size;
	len = vec_len(*vec.v) * size;

	memmove(*vec.v, *vec.v + off, ext);
	memset(*vec.v + ext, 0, len - ext);

	(*vec.z)[-1] = min;
}

int
vec_splice(void *destp, size_t pos, void const *src, size_t nmemb, size_t size)
{
	union vec dest = {.p = destp};
	size_t ext = 0;
	size_t len = 0;
	size_t off = 0;
	size_t mem = 0;
	int err = 0;

	if (off > vec_len(*dest.v)) return EINVAL;

	ext = nmemb * size;
	len = vec_len(*dest.v) * size;
	off = pos * size;

	if (len + ext >= vec_mem(*dest.v)) {
		mem = vec_mem(*dest.v);
		while (mem <= len + ext) mem *= 2;

		err = vec_resize(dest.v, mem, size);
		if (err) return err;
	}

	memmove(*dest.v + off + ext,
		*dest.v + off,
		len - off);

	memcpy(*dest.v + off, src, ext);

	(*dest.z)[-1] += nmemb;

	return 0;
}

void
vec_truncat(void *vecp, size_t off, size_t size)
{
	union vec vec = {.p = vecp};
	memset(*vec.v + off * size,
		0,
		(vec_len(*vec.v) - off) * size);
	(*vec.z)[-1] = off;
}

int
vec_transfer(void *destp, void const *data, size_t len, size_t size)
{
	vec_truncat(destp, 0, size);
	return vec_splice(destp, 0, data, len, size);
}
