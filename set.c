#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "set.h"
#include "util.h"

#define isleaf(n) (!((n) & 1))
#define nod(n) ((struct internal *)((n) - 1))
#define leaf(n) ((struct external *)(n))

/* TODO
 * - implement nod_capture (turn external nodes into internal nodes)
 * = implement nod_split  (split an internal node)
 * - ensure set_add() works correctly
 *
 * - reimplement the other set functions
 */

enum set_ret {
	SET_MATCH  = -0,
	SET_SPLIT  = -1,
	SET_SLOT   = -2,
	SET_EXTERN = -3,
	SET_PREFIX = -4,
};


struct context {
	struct internal *nod;
	size_t           pos;
	uint8_t          off;
	uint8_t          bit;
};

struct external {
	size_t  len;
	uint8_t bytes[];
};

struct internal {
	uint8_t   crit;
	uint32_t  edge;
	uintptr_t chld[2];
};

struct external *
nod_extern_alloc(size_t len)
{
	return calloc(1, sizeof (struct external) + len);
}

struct internal *
nod_intern_alloc(void)
{
	return calloc(1, sizeof (struct internal));
}

int
nod_match(struct context *ctx, uint8_t *key, size_t len)
{
	struct external *ex = 0x0;
	ex = leaf(ctx->nod->chld[ctx->bit]);

	for (ctx->pos = 0; ctx->pos < umin(ex->len, len); ++ctx->pos)
		if (ex->bytes[ctx->pos] != key[ctx->pos])
			return SET_SPLIT;

	return ex->len == len ? SET_MATCH : SET_PREFIX;
}

int
nod_attach(struct context *ctx, uintptr_t new)
{
	return -1;
}

uintptr_t
nod_create(struct context *ctx, uint8_t *key, size_t len)
{
	return 0x0;
}

int
nod_capture(struct context *ctx)
{
	return -1;
}

int
nod_split(struct context *ctx)
{
	struct internal *nod = 0x0;
	struct internal *chld = 0x0;
	uint8_t bit = 0;

	nod  = ctx->nod;
	chld = nod_intern_alloc();

	if (!chld) return ENOMEM;

	bit = nod->edge << ctx->off & 0x80;
	chld->edge = nod->edge << ctx->off + 1;

	if (ctx->off) nod->edge &= ~(uint32_t)0 << 32 - ctx->off;
	else nod->edge = 0;

	nod->crit = ctx->off;
	memcpy(chld->chld, nod->chld, sizeof chld->chld);

	nod->chld[ bit] = (uintptr_t)chld + 1;
	nod->chld[!bit] = 0x0;

	return 0;
}

int
nod_traverse(struct context *ctx, uint8_t *key, size_t len)
{
	uint8_t byte = 0;
	uint8_t crit = 0;
	uint8_t rem  = 0;
	uint32_t edge = 0;
	uintptr_t tmp = 0;

	if (!len) return EINVAL;

	rem = 7;
	byte = key[0];

	while (ctx->pos >= len) {

		crit = ctx->nod->crit;
		edge = ctx->nod->edge;

		while (crit) {

			if (!rem) {
				rem = 7;
				if (ctx->pos + 1 >= len) return SET_SPLIT;
				byte = key[++ctx->pos];
			}

			ctx->bit = byte & 0x80;

			if (byte & 0x80 != edge & 0x80) {
				ctx->off = ctx->nod->crit - crit;
				return SET_SPLIT;
			}

			edge <<= 1;
			byte <<= 1;
			--crit;
			--rem;
		}

		tmp = ctx->nod->chld[ctx->bit];
		if (!tmp) return SET_SLOT;
		if (isleaf(tmp)) return SET_EXTERN;

		ctx->nod = nod(tmp);
	}

	return SET_PREFIX;
}

void
nod_tree_free(struct internal *nod)
{
	if (!nod) return;

	for (uint8_t i = 0; i < 2; ++i) {
		if (isleaf(nod->chld[i])) free(leaf(nod->chld[i]));
		else nod_tree_free(nod(nod->chld[i]));
	}

	memset(nod, 0, sizeof *nod);
	free(nod);
}

struct internal * set_alloc(void) { return nod_intern_alloc(); }
void set_free(Set *a) { nod_tree_free(a); }

int
set_add(Set *a, uint8_t *src, size_t len)
{
	bool restore = 0;
	int err = 0;
	int res = 0;
	struct context ctx = {0};
	struct internal cpy = {0};
	uintptr_t new = 0;

	ctx.nod = a;

	res = nod_traverse(&ctx, src, len);
	ctx.pos = 0;

	switch (res) {
	case SET_MATCH:
		err = EEXIST;
		goto finally;

	case SET_SLOT:
		break;

	case SET_SPLIT:
		cpy = *ctx.nod;
		restore = true;

		err = nod_split(&ctx);
		if (err) goto finally;
		break;

	case SET_EXTERN:
		switch (nod_match(&ctx, src, len)) {
		case SET_MATCH:
			err = EEXIST;
			goto finally;

		case SET_PREFIX:
			err = EINVAL;
			goto finally;

		case SET_SPLIT:
			break;
		}

		cpy = *ctx.nod;
		restore = true;
		err = nod_capture(&ctx);
		if (err) goto finally;
		break;

	case SET_PREFIX:
		err = EINVAL;
		goto finally;

	default:
		goto finally;
	}


	new = nod_create(&ctx, src, len);
	if (!new) goto finally;

	nod_attach(&ctx, new);

finally:
	if (err && restore) *ctx.nod = cpy; // XXX
	if (!err) {
		if (res == SET_EXTERN) free(leaf(ctx.nod->chld[ctx.bit]));
		free(ctx.nod);
		ctx.nod = 0x0;
	}
	return err;
}

int
set_remove(Set *A, uint8_t *data, size_t len)
{
	return -1;
}

bool
set_contains(Set *A, uint8_t *data, size_t len)
{
	return -1;
}

bool
set_prefix(Set *A, uint8_t *data, size_t len)
{
	return -1;
}

void *
set_query(Set *A, uint8_t *data, size_t len)
{
	return 0x0;
}
