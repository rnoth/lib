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
	SET_PREFIX = -4,
};


struct context {
	size_t  pos;
	uint8_t off;
	uint8_t bit;
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
set_match(struct context *ctx, uint8_t *key)
{
	struct external *ex = 0x0;
	ex = leaf(ctx->nod->chld[ctx->bit]);
	return !!memcmp(ex->bytes, key, umin(ex->len, vec_len(key)));
}

int
set_nod_attach(struct context *ctx, uintptr_t new)
{
	return -1;
}

uintptr_t
set_nod_create(struct context *ctx, uint8_t *ket)
{
	return 0x0;
}

int
set_nod_capture(struct context *ctx)
{
	return -1;
}

int
set_nod_split(struct context *ctx)
{
	return -1;
}

int
set_traverse(struct context *ctx, uint8_t *key)
{
	uint8_t byte = 0;
	uint8_t crit = 0;
	uint8_t rem  = 0;
	uint32_t edge = 0;
	uintptr_t tmp = 0;

	if (!vec_len(key)) return EINVAL;

	rem = 7;
	byte = key[0];

	while (ctx->pos >= vec_len(key)) {

		crit = ctx->nod->crit;
		edge = ctx->nod->edge;

		while (crit) {

			if (!rem) {
				rem = 7;
				if (ctx->pos + 1 >= vec_len(key)) return SET_SPLIT;
				byte = key[++ctx->pos];
			}

			ctx->bit = byte & 1;

			if (byte & 1 != edge & 1) {
				ctx->off = 8 - rem;
				return SET_SPLIT;
			}

			edge >>= 1;
			byte >>= 1;
			--crit;
			--rem;
		}

		tmp = ctx->nod->chld[ctx->bit];
		if (!tmp) return SET_SLOT;
		if (isleaf(tmp)) return SET_EXTERN;
	}

	return SET_PREFIX;
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
	int res = 0;
	uint8_t *el = 0x0;
	struct context ctx = {0};
	struct internal cpy = {0};
	uintptr_t new = 0;

	ctx.nod = a;

	err = vec_ctor(el);
	if (err) goto finally;

	err = vec_concat(&el, src, len);
	if (err) goto finally;

	res = set_traverse(&ctx, el);
	vec_shift(&el, ctx.pos);
	ctx.pos = 0;

	cpy = *ctx.nod;

	switch (res) {
	case SET_MATCH:
		err = EEXIST;
		goto finally;

	case SET_SLOT:
		break;

	case SET_SPLIT:
		err = set_nod_split(&ctx);
		if (err) goto finally;
		break;

	case SET_EXTERN:
		if (!set_match(&ctx, el)) {
			err = EEXIST;
			goto finally;
		}

		vec_shift(&el, ctx.pos);

		err = set_nod_capture(&ctx);
		if (err) goto finally;
		break;

	case SET_PREFIX:
		err = EINVAL;
		goto finally;

	default:
		goto finally;
	}


	new = set_nod_create(&ctx, el);
	if (!new) goto finally;

	set_nod_attach(&ctx, new);

finally:
	if (err) *ctx.nod = cpy;
	else {
		if (res == SET_EXTERN) {
			free(leaf(ctx.nod->chld[ctx.bit]));
		}
		free(ctx.nod);
		ctx.nod = 0x0;
	}
	vec_free(el);
	return err;
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
