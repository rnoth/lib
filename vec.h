#ifndef _vector_
#define _vector_
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define len(vec) (((size_t *)(vec))[-1]) // XXX

#define vec_ctor(INST) vec_alloc(&(INST), sizeof *INST)

#define vec_foreach(VAR, VEC)					\
	for (char *_vec = (void *)(VEC), *_p=(void*)1;		\
			_vec && _p;				\
			_p = 0)					\
	for (size_t _i=0,_j=0, _siz = sizeof *(VEC);		\
			_i < vec_len(_vec);			\
			++_i, _j = _i)				\
	for (char _q = 1; _q; _q = 0)				\
	for (VAR = (void*)(_vec + _i * _siz);			\
			_i = _q ? len(_vec) : _j, _q;		\
			_q = 0)

#define vec_assert(prop) do if (!(prop)) {			\
		fprintf(stderr,					\
			"error: invalid pointer passed to %s\n",\
			__func__);				\
		abort();					\
} while (0)

#define vec_check(vecv) do {                                  \
	vec_assert(*vecv != 0x0);                             \
	vec_assert(vec_len(*vecv) <= vec_mem(*vecv));                 \
	vec_assert(sizeof **vecv < vec_mem(*vecv));               \
	vec_assert(vec_len(*vecv) * sizeof **vecv <= vec_mem(*vecv)); \
} while (0)

static inline
size_t
vec_len(void const *v)
{
	size_t ret;
	union {
		size_t const *z;
		void const *v;
	} vec = { .v = v };
	memcpy(&ret, vec.z - 1, sizeof ret);
	return ret;
}

size_t  vec_mem(void const *, size_t);

int     vec_alloc    (void *, size_t);
int     vec_append   (void *, void const *, size_t);
void *  vec_clone    (void const *, size_t);
int     vec_concat   (void *, void const *, size_t, size_t);
int     vec_copy     (void *, void *, size_t);
void    vec_delete   (void *, size_t, size_t);
void    vec_elim     (void *, size_t, size_t, size_t);
int     vec_insert   (void *, void const *, size_t, size_t);
int     vec_join     (void *, void const *, size_t);
void    vec_free     (void *);
void    vec_pop      (void *, void *, size_t);
int     vec_resize   (void *, size_t, size_t);
void    vec_shift    (void *, size_t, size_t);
void    vec_slice    (void *, size_t, size_t, size_t);
int     vec_splice   (void *, size_t, void const *, size_t, size_t);
int     vec_transfer (void *, void const *, size_t, size_t);
void    vec_truncat  (void *, size_t, size_t);

#define vec_concat_arr(dest_ptr, src) vec_concat(dest_ptr, src, arr_len(src))
#define vec_mem(vec) vec_mem(vec, sizeof *vec)

#define vec_append(vec_ptr, src)                vec_append(vec_ptr, src,               sizeof **vec_ptr)
#define vec_clone(vec)                          vec_clone(vec,                         sizeof *vec)
#define vec_concat(dest_ptr, src, len)          vec_concat(dest_ptr, src, len,         sizeof **dest_ptr)
#define vec_copy(dest_ptr, src)                 vec_copy(dest_ptr, src,                sizeof **dest_ptr)
#define vec_delete(vec_ptr, ind)                vec_delete(vec_ptr, ind,               sizeof **vec_ptr)
#define vec_elim(vec_ptr, ind, nmemb)           vec_elim(vec_ptr, ind, nmemb,          sizeof **vec_ptr)
#define vec_insert(dest_ptr, src, ind)          vec_insert(dest_ptr, src, ind,         sizeof **dest_ptr)
#define vec_join(dest_ptr, src)                 vec_join(dest_ptr, src,                sizeof **dest_ptr)
#define vec_pop(dest, src_ptr)                  vec_pop(dest, src_ptr,                 sizeof **src_ptr)
#define vec_resize(vec_ptr, n)                  vec_resize(vec_ptr,n * sizeof**vec_ptr,sizeof **vec_ptr)
#define vec_shift(vec_ptr, off)                 vec_shift(vec_ptr, off,                sizeof **vec_ptr)
#define vec_slice(vec_ptr, off, nmemb)          vec_slice(vec_ptr, off, nmemb,         sizeof **vec_ptr)
#define vec_splice(dest_ptr, ind, src, nmemb)   vec_splice(dest_ptr, ind, src, nmemb,  sizeof **dest_ptr)
#define vec_transfer(dest_ptr, src, nmemb)      vec_transfer(dest_ptr, src, nmemb,     sizeof **dest_ptr)
#define vec_truncat(vec_ptr, off)               vec_truncat(vec_ptr, off,              sizeof **vec_ptr)

#endif
