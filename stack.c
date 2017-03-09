#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "stack.h"

static int expandstack(Stack *);


void
maps(Stack *st, void (*fun)())
{
	size_t i;

	i = st->h;
	while (i --> 0) {
		fun(st->v[i]);
	}
}

void *
stack_pop(Stack *st)
{
	void *ret;

	if (!st->h) return NULL;
	ret = st->v[--st->h];

	return ret;
}

int
stack_push(Stack *st, void *elem)
{
	if (st->h * sizeof st->h >= st->m && expandstack(st)) return ENOMEM;
	st->v[st->h] = elem;
	++st->h;

	return 0;
}

int
expandstack(Stack *st)
{
	void *tmp;

	tmp = realloc(st->v, st->m * 2);
	if (!tmp) return ENOMEM;
	st->v = tmp;
	st->m *= 2;

	return 0;
}
