#ifndef _util_
#define _util_
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

/* safe macros */
#define arr_len(arr)    (sizeof arr / sizeof *arr)
#define bit(off)        (1UL << (off))
#define cpy(dest,src)   (memcpy((dest), (src), sizeof *(dest)))
#define die(blame)      do { perror(blame); abort(); } while (0);

#define arr_foreach(VAR, ARR) 				\
	for (size_t _i=0, _j=0;				\
			_i < arr_len(ARR);		\
			++_i, _j = _i)			\
	for (bool _q = true; _q; _q = false)		\
	for (VAR = ARR[_i];				\
			_i = _q ? arr_len(ARR) :_j,_q;	\
			_q = 0)

static inline
unsigned long
umin(unsigned long a, unsigned long b)
{
	return a < b ? a : b;
}

static inline
unsigned long
umax(unsigned long a, unsigned long b)
{
	return a > b ? a : b;
}

static inline
bool
addz_overflows(size_t a, size_t b)
{
	return a + b < a || a + b < b;
}

static inline
bool
subz_overflows(size_t a, size_t b)
{
	return a - b > a;
}

static inline
bool
mulz_overflows(size_t a, size_t b)
{
	return a * b < a || a * b < b;
}

#endif /* _util_ */
