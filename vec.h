#ifndef _vector_
#define _vector_
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define vec_foreach(VAR, VEC)                                   \
	for (char *_vec = (void *)(VEC), _ = 0;                 \
			_vec && _ < 1;                          \
			++_)                                    \
	for (size_t _i=0,_j=0, _siz = sizeof *(VEC);            \
			_i < vec_len(_vec);                     \
			++_i, _j = _i)                          \
	for (int _ = 0; _ < 1; ++_)                             \
	for (VAR = (void*)(_vec + _i * _siz);                   \
			_i = !_ ? vec_len(_vec) : _j, !_;       \
			++_)

static inline
size_t
vec_len(void const *v)
{
	size_t ret;
	memcpy(&ret, (char *)v - sizeof ret, sizeof ret);
	return ret;
}

static inline
size_t
vec_mem(void const *v)
{
	size_t ret;
	memcpy(&ret, (char *)v - sizeof ret * 2, sizeof ret);
	return ret;
}

static inline
void
vec_cat(void *d, void const *s, size_t size)
{
	memcpy((char*)d + vec_len(d) * size, s, size * vec_len(s));
	memcpy((size_t*)d - 1,
	       (size_t[]){vec_len(d) + vec_len(s)},
	       sizeof (size_t));
}


static inline
void
vec_get(void *dst, void *src, size_t size)
{
	memcpy(dst,(char *)src+(vec_len(src)-1)*size, size);
	memset((char*)src+(vec_len(src)-1)*size, 0, size);
	memcpy((char*)src - sizeof (size_t),
	       (size_t[]){vec_len(src)-1},
	       sizeof (size_t));
}

static inline
void
vec_put(void *dst, void const *src, size_t size)
{
	memcpy((char *)dst + vec_len(dst) * size, src, size);
	memcpy((char *)dst - sizeof (size_t),
	       (size_t[]){vec_len(dst)+1},
	       sizeof (size_t));
}

static inline
void
vec_zero(void *vec, size_t size)
{
	memset((char*)vec-sizeof(size_t),
	       0,
	       sizeof(size_t)+(vec_len(vec)-1)*size);
}

void *  vec_alloc    (size_t, size_t);
int     vec_append   (void *, void const *, size_t);
void *  vec_clone    (void const *, size_t);
int     vec_concat   (void *, void const *, size_t, size_t);
int     vec_copy     (void *, void *, size_t);
int     vec_ctor     (void *, size_t);
void    vec_delete   (void *, size_t, size_t);
void    vec_elim     (void *, size_t, size_t, size_t);
int     vec_insert   (void *, void const *, size_t, size_t);
int     vec_join     (void *, void const *, size_t);
void    vec_free     (void *);
void *  vec_new      (size_t);
void    vec_pop      (void *, void *, size_t);
int     vec_resize   (void *, size_t, size_t);
void    vec_shift    (void *, size_t, size_t);
void    vec_slice    (void *, size_t, size_t, size_t);
int     vec_splice   (void *, size_t, void const *, size_t, size_t);
int     vec_transfer (void *, void const *, size_t, size_t);
void    vec_truncat  (void *, size_t, size_t);

#define vec_alloc(type, len) vec_alloc(sizeof (type), len)
#define vec_concat_arr(dest_ptr, src) vec_concat(dest_ptr, src, arr_len(src))
#define vec_ctor(INST) vec_ctor(&(INST), sizeof *INST)
#define vec_new(type) vec_new(sizeof (type))
#define vec_put(dst, src) vec_put(dst, src, sizeof *dst)
#define vec_get(dst, src) vec_get(dst, src, sizeof *src)
#define vec_zero(vec)     vec_zero(vec, sizeof *vec)
#define vec_cat(dst, src) vec_cat(dst, src, sizeof *dst)

#define vec_append(vec_ptr, src)       vec_append(vec_ptr, src,               sizeof **vec_ptr)
#define vec_clone(vec)                 vec_clone(vec,                         sizeof *vec)
#define vec_concat(dest_ptr, src, len) vec_concat(dest_ptr, src, len,         sizeof **dest_ptr)
#define vec_copy(dest_ptr, src)        vec_copy(dest_ptr, src,                sizeof **dest_ptr)
#define vec_delete(vec_ptr, ind)       vec_delete(vec_ptr, ind,               sizeof **vec_ptr)
#define vec_elim(vec_ptr, ind, nmemb)  vec_elim(vec_ptr, ind, nmemb,          sizeof **vec_ptr)
#define vec_insert(dest_ptr, src, ind) vec_insert(dest_ptr, src, ind,         sizeof **dest_ptr)
#define vec_join(dest_ptr, src)        vec_join(dest_ptr, src,                sizeof **dest_ptr)
#define vec_pop(dest, src_ptr)         vec_pop(dest, src_ptr,                 sizeof **src_ptr)
#define vec_resize(vec_ptr, n)         vec_resize(vec_ptr,n * sizeof**vec_ptr,sizeof **vec_ptr)
#define vec_shift(vec_ptr, off)        vec_shift(vec_ptr, off,                sizeof **vec_ptr)
#define vec_slice(vec_ptr, off, nmemb) vec_slice(vec_ptr, off, nmemb,         sizeof **vec_ptr)
#define vec_splice(dest_ptr, i, s, n)  vec_splice(dest_ptr, i, s, n,          sizeof **dest_ptr)
#define vec_transfer(dest_ptr, s, n)   vec_transfer(dest_ptr, s, n,           sizeof **dest_ptr)
#define vec_truncat(vec_ptr, off)      vec_truncat(vec_ptr, off,              sizeof **vec_ptr)

#endif
