#ifndef _lib_arr_h_
#define _lib_arr_h_
#include <stdlib.h>
#include <string.h>

static inline
void *
arr_alloc(size_t size, size_t len)
{
	size_t mem; 
	mem = size * len + sizeof (size_t);
	return (size_t*)calloc(1, mem) + 1;
}

static inline
void
arr_free(void *a)
{
	free((size_t *)a - 1);
}

static inline
size_t
arr_len(void const *a)
{
	size_t ret;
	memcpy(&ret, (size_t *)a - 1, sizeof ret);
	return ret;
}

static inline
void
arr_cat(void *d, void const *s, size_t size)
{
	memcpy((char*)d + arr_len(d) * size, s, size * arr_len(s));
	memcpy((size_t*)d - 1,
	       (size_t[]){arr_len(d) + arr_len(s)},
	       sizeof (size_t));
}


static inline
void
arr_get(void *dst, void *src, size_t size)
{
	memcpy(dst,(char *)src+(arr_len(src)-1)*size, size);
	memset((char*)src+(arr_len(src)-1)*size, 0, size);
	memcpy((char*)src - sizeof (size_t),
	       (size_t[]){arr_len(src)-1},
	       sizeof (size_t));
}

static inline
void
arr_put(void *dst, void const *src, size_t size)
{
	memcpy((char *)dst + arr_len(dst) * size, src, size);
	memcpy((char *)dst - sizeof (size_t),
	       (size_t[]){arr_len(dst)+1},
	       sizeof (size_t));
}

static inline
void
arr_zero(void *arr, size_t size)
{
	memset((size_t*)arr-1,
	       0,
	       sizeof(size_t)+(arr_len(arr)-1)*size);
}

#define arr_cat(d, s) arr_cat(d, s, sizeof *d)
#define arr_get(d, s) arr_get(d, s, sizeof *s)
#define arr_put(d, s) arr_put(d, s, sizeof *d)
#define arr_zero(a)   arr_zero(a, sizeof *a)
#endif /* _lib_arr_h_ */
