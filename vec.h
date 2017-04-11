#ifndef _vector_
#define _vector_
#include <stddef.h>

#define VECSIZ 16

#define len(vec) (((size_t *)(vec))[-3])
#define siz(vec) (((size_t *)(vec))[-2])
#define mem(vec) (((size_t *)(vec))[-1])

#define Vector(Type) Type

#define make_vector(INST) vec_alloc(&(INST), sizeof *INST)

#define mapv(var, VEC)						\
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

int	vec_alloc	(void *, size_t);
int	vec_append	(void *, void const *);
void *	vec_clone	(void const *);
int	vec_copy	(void *, void *);
int	vec_concat	(void *, void const *, size_t);
void	vec_delete	(void *, size_t);
int	vec_insert	(void *, void const *, size_t);
int	vec_join	(void *, void const *);
void	vec_free	(void *);
void	vec_shift	(void *, size_t);
void	vec_slice	(void *, size_t, size_t);
void	vec_truncate	(void *, size_t);
int	vec_transfer	(void *, void const *, size_t);

#endif
