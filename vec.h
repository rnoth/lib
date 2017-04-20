#ifndef _vector_
#define _vector_
#include <stddef.h>
#include <stdlib.h>

#define VECSIZ 16

#define len(vec) (((size_t *)(vec))[-3])
#define siz(vec) (((size_t *)(vec))[-2])
#define mem(vec) (((size_t *)(vec))[-1])

#define Vector(Type) Type

#define vec_ctor(INST) vec_alloc(&(INST), sizeof *INST)

#define vec_foreach(var, VEC)					\
	for (char *_vec = (void *)(VEC), *_p=(void*)1;		\
			_vec && _p;				\
			_p = 0)					\
	for (size_t _i=0,_j=0, _siz = sizeof *(VEC);		\
			_i < len(_vec);				\
			++_i, _j = _i)				\
	for (char _q = 1; _q; _q = 0)				\
	for (var = (void*)(_vec + _i * _siz);			\
			_i = _q ? len(_vec) : _j, _q;		\
			_q = 0)

#define vec_assert(prop) do if (!(prop)) {			\
		fprintf(stderr,					\
			"error: invalid pointer passed to %s\n",\
			__func__);				\
		abort();					\
} while (0)

#define vec_check(vecv) do {			\
	vec_assert(*vecv != 0x0);			\
	vec_assert(len(*vecv) <= mem(*vecv));	\
	vec_assert(siz(*vecv) < mem(*vecv));		\
	vec_assert(len(*vecv) * siz(*vecv) <= mem(*vecv)); \
} while (0)

static inline size_t vec_len(void const *vec) { return (((size_t *)(vec))[-3]); }
static inline size_t vec_siz(void const *vec) { return (((size_t *)(vec))[-2]); }
static inline size_t vec_mem(void const *vec) { return (((size_t *)(vec))[-1]); }

int	vec_alloc	(void *, size_t);
int	vec_append	(void *, void const *);
void *	vec_clone	(void const *);
int	vec_concat	(void *, void const *, size_t);
int	vec_copy	(void *, void *);
void	vec_delete	(void *, size_t);
void	vec_elim	(void *, size_t, size_t);
int	vec_insert	(void *, void const *, size_t);
int	vec_join	(void *, void const *);
void	vec_free	(void *);
void	vec_shift	(void *, size_t);
void	vec_slice	(void *, size_t, size_t);
int	vec_splice	(void *, size_t, void const *, size_t);
int	vec_transfer	(void *, void const *, size_t);
void	vec_truncate	(void *, size_t);

#endif
