#ifndef _util_
#define _util_
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

/* safe macros */
#define array_len(arr)  (sizeof arr / sizeof *arr)
#define bit(off)        (1UL << (off))
#define cpy(dest,src)   (memcpy((dest), (src), sizeof *(dest)))
#define die(blame)      do { perror(blame); exit(1); } while (0);

#define arr_foreach(VAR, ARR) 				\
	for (size_t _i=0, _j=0;				\
			_i < arr_len(ARR);		\
			++_i, _j = _i)			\
	for (bool _q = true; _q; _q = false)		\
	for (VAR = ARR[_i];				\
			_i = _q ? arr_len(ARR) :_j,_q;	\
			_q = 0)

#define repeat(ntimes) for (size_t _repeat_counter = 0;   \
				_repeat_counter < ntimes; \
				++_repeat_counter)

#define iterate(var, ntimes) for (size_t var = 0; \
				  var < ntimes;   \
				  ++var)

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
unsigned long
ucmp(unsigned long a, unsigned long b)
{
	return a > b ? 1 : a < b ? -1 : 0;
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
