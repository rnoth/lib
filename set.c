#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "set.h"
#include "util.h"
#include "vec.h"

#define isleaf(n) (!((n) & 1))
#define nod(n) ((struct internal *)((n) - 1))
#define leaf(n) ((struct external *)(n))

enum set_ret {
	SET_MATCH  = -0,
	SET_SPLIT  = -1,
	SET_SLOT   = -2,
	SET_EXTERN = -3,
};


struct context {
	uint8_t off;
	uint8_t mask;
	struct internal *nod;
};

struct external {
	size_t len;
	uint8_t bytes[];
};

struct internal {
	uint8_t crit;
	uint32_t edge;
	uintptr_t chld[2];
};

union edge {
	uint32_t *w;
	uint8_t *y;
};

struct external *
set_extern_alloc(size_t len)
{
	return calloc(1, sizeof (struct external) + len);
}

struct internal *
set_intern_alloc(void)
{
	return calloc(1, sizeof (struct internal));
}

int
set_attach(struct context *ctx, struct internal *nod)
{
	return -1;
}

int
set_split(struct context *ctx)
{
	return -1;
}

int
set_traverse(struct context *ctx, uint8_t *key)
{
	union edge ed = { .w = &ctx->nod->edge };
	size_t off = 0;
	uint8_t bit = 0;
	uint8_t byte = 0;
	uint8_t crit = ctx->nod->crit;
	uint8_t i = 0;
	uintptr_t tmp;

	while (vec_len(key)) {
		byte = key[off];
		if (ctx->mask) byte &= ctx->mask;
		
		for (i = 0; i < 4; ++i) {
			if (crit < 7) byte &= ~0 << crit;
			if (byte != ed.y[i]) goto split;

			if (++off >= vec_len(key)) goto done;
			if (crit < 8) break;

			byte = key[off];
			crit -= 8;
			ctx->off += 8;
		}

		ctx->off = 0;
		if (crit < 7) ctx->mask = ~0 >> crit;

		bit = (key[off] >> -~crit) & 1;

		tmp = ctx->nod->chld[bit];
		if (!tmp) goto slot;
		if (isleaf(tmp)) goto ext;

		ctx->nod = nod(tmp);
	}

done:
	if (ctx->nod->crit != ctx->off) goto split;

	return SET_MATCH;
split:
	return SET_SPLIT;
slot:
	return SET_SLOT;
ext:
	return SET_EXTERN;

}

void
set_tree_free(struct internal *nod)
{
	if (!nod) return;

	for (uint8_t i = 0; i < 2; ++i) {
		if (isleaf(nod->chld[i])) free(leaf(nod->chld[i]));
		else set_tree_free(nod(nod->chld[i]));
	}

	memset(nod, 0, sizeof *nod);
	free(nod);
}

struct internal *
set_alloc(void)
{
	return set_intern_alloc();
}

void
set_free(Set *a)
{
	set_tree_free(a);
}

int	set_adds   (Set *A, char *s) { return set_add   (A, s, strlen(s) + 1); }
int	set_rms	   (Set *A, char *s) { return set_rm    (A, s, strlen(s) + 1); }
bool	set_membs  (Set *A, char *s) { return set_memb  (A, s, strlen(s) + 1); }
bool	set_prefixs(Set *A, char *s) { return set_prefix(A, s, strlen(s)); }
void *	set_querys (Set *A, char *s) { return set_query (A, s, strlen(s)); }

int
set_add(Set *a, void *src, size_t len)
{
	int err = 0;
	uint8_t *elem = 0x0;
	struct context ctx = {0};

	ctx.nod = a;

	err = vec_ctor(elem);
	if (err) goto finally;

	err = vec_concat(&elem, src, len);
	if (err) goto finally;

	switch (set_traverse(&ctx, elem))
	case SET_MATCH:
		err = EEXIST;
		goto finally;
	case SET_SPLIT:
		err = set_nod_split(ctx);
		if (err) goto finally;
		break;
	case SET_EXTERN:
		err = 
	}

finally:
	vec_free(elem);

	return -1;
}

int
set_rm(Set *A, void *data, size_t len)
{
	return -1;
}

bool
set_memb(Set *A, void *data, size_t len)
{
	return -1;
}

bool
set_prefix(Set *A, void *data, size_t len)
{
	return -1;
}

void *
set_query(Set *A, void *data, size_t len)
{
	return 0x0;
}
