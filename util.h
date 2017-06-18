#ifndef _util_
#define _util_
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

#define array_len(arr)  (sizeof arr / sizeof *arr)
#define bit(off)        (1UL << (off))
#define cpy(dest,src)   (memcpy((dest), (src), sizeof *(dest)))
#define die(blame)      do { perror(blame); exit(1); } while (0);

static inline
void
ptr_swap(void *a, void *b)
{
	void **aa = a, **bb = b, *tmp;

	tmp = *bb;
	*bb = *aa;
	*aa = tmp;
}

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

#endif /* _util_ */
