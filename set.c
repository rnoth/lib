#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "set.h"
#include "util.h"

static inline bool isleaf(uintptr_t n) { return n ? n ^ 1 : false; }
static inline bool isnod(uintptr_t n) { return n ? n & 1 : false; }
static inline struct internal *nod(uintptr_t n) { return (void *)(n - 1); }
static inline struct external *leaf(uintptr_t n) { return (void *)n; }

/* TODO
 * - implement nod_capture (turn external nodes into internal nodes)
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
	uintptr_t nod;
	size_t    pos;
	uint8_t   off  : 5;
	uint8_t   shft : 3;
	uint8_t   bit  : 1;
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

static enum set_ret nod_walk(struct context *, uint8_t *, size_t);
static void nod_tree_free(struct internal *);

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

void
nod_attach(struct context *ctx, struct external *new)
{
	nod(ctx->nod)->chld[ctx->bit] = (uintptr_t)new;
}

void
nod_align_internal(struct context *ctx, struct internal *anc, struct internal *rem)
{
}

void
nod_align_external(struct context *ctx, struct internal *anc, struct external *rem)
{
}

void
nod_realign(struct context *ctx, struct internal *anc, uintptr_t rem)
{
	if (isnod(rem)) nod_align_internal(ctx, anc, nod(rem));
	else if (isleaf(rem)) nod_align_external(ctx, anc, leaf(rem));
}	

int
nod_capture(struct context *ctx)
{
	return -1;
}

void
nod_init(struct external *new, struct context *ctx, uint8_t *key, size_t len)
{
	memcpy(new->bytes, key + ctx->pos, len - ctx->pos);
	new->len = len - ctx->pos;
}

enum set_ret
nod_match(struct context *ctx, uint8_t *key, size_t len)
{
	uint8_t tmp = 0;
	size_t ind = 0;
	struct external *ex = leaf(ctx->nod);

	for (ctx->pos = 0; ctx->pos < umin(ex->len, len); ++ctx->pos, ++ind) {
		for (tmp = ex->bytes[ind] ^ key[ctx->pos], ctx->off = 0;
		     tmp;
		     tmp >>= 1, ++ctx->off)
			;
		if (ctx->off) return SET_SPLIT;
	}

	return ex->len == len ? SET_MATCH : SET_PREFIX;
}

int
nod_split(struct context *ctx)
{
	struct internal *in = 0x0;
	struct internal *chld = 0x0;
	uint8_t bit = 0;

	in   = nod(ctx->nod);
	chld = nod_intern_alloc();

	if (!chld) return ENOMEM;

	bit = in->edge << ctx->off & 0x80;
	chld->edge = in->edge << ctx->off + 1;

	if (ctx->off) in->edge &= ~(uint32_t)0 << 32 - ctx->off;
	else in->edge = 0;

	in->crit = ctx->off;
	memcpy(chld->chld, in->chld, sizeof chld->chld);

	in->chld[ bit] = (uintptr_t)chld + 1;
	in->chld[!bit] = 0x0;

	return 0;
}

enum set_ret
nod_traverse(struct context *ctx, uint8_t *key, size_t len)
{
	int res = 0;
	uintptr_t tmp = 0;

	while (ctx->pos <= len) {

		res = nod_walk(ctx, key, len);
		if (res) return res;

		tmp = nod(ctx->nod)->chld[ctx->bit];
		if (!tmp) return SET_SLOT;

		ctx->nod = tmp;
		if (isleaf(tmp)) return SET_EXTERN;
	}

	return SET_PREFIX;
}

void
nod_tree_free(struct internal *n)
{
	if (!n) return;

	for (uint8_t i = 0; i < 2; ++i) {
		if (isleaf(n->chld[i])) free(leaf(n->chld[i]));
		else nod_tree_free(nod(n->chld[i]));
	}

	memset(n, 0, sizeof *n);
	free(n);
}

enum set_ret
nod_walk(struct context *ctx, uint8_t *key, size_t len)
{
	uint8_t  byte = key[ctx->pos];
	uint8_t  rem = 0;
	struct internal *in = nod(ctx->nod);
	uint8_t  crit = in->crit;
	uint32_t edge = in->edge;

	ctx->bit = byte & 0x80;

	for (; crit; --crit, edge <<= 1) {

		if (!rem--) {

			rem = 7;
			if (ctx->pos + 1 > len) return SET_PREFIX;
			byte = key[++ctx->pos];

		} else byte <<= 1;

		ctx->bit = byte & 0x80;

		if (byte & 0x80 != edge & 0x80) {
			ctx->off = in->crit - crit + 1;
			return SET_SPLIT;
		}
	}

	ctx->shft = umin(rem - 1, 7);

	return 0;
}

struct internal *
set_alloc(void)
{
	struct internal *ret;
	ret = nod_intern_alloc();
	ret = (void *)((char *)ret + 1);
	return ret;
}

void
set_free(Set *t)
{
	nod_tree_free(nod((uintptr_t)t));
}

int
set_add(Set *t, uint8_t *src, size_t len)
{
	int err = 0;
	enum set_ret res = 0;
	struct context ctx = {0};
	struct external *new = 0x0;

	if (!len) return EINVAL;
	if (!src) return EFAULT;
	if (!t)   return EFAULT;

	new = nod_extern_alloc(len);
	if (!new) goto finally;

	ctx.nod = (uintptr_t)t;

	res = nod_traverse(&ctx, src, len);
	switch (res) {
	case SET_PREFIX: err = EILSEQ; goto finally;
	case SET_SLOT:   break;
	case SET_EXTERN: break;
	case SET_SPLIT:
		err = nod_split(&ctx);
		if (err) goto finally;
		break;
	}

	if (res == SET_EXTERN) {
		res = nod_match(&ctx, src, len);
		switch (res) {
		case SET_MATCH:  return EEXIST;
		case SET_PREFIX: return EILSEQ;
		case SET_SPLIT:
			err = nod_capture(&ctx);
			if (err) goto finally;
		}
	}

	nod_init(new, &ctx, src, len);
	nod_attach(&ctx, new);

finally:
	if (err) free(new);
	return err;
}

int
set_remove(Set *t, uint8_t *key, size_t len)
{
	struct internal *anc = 0x0;
	struct internal *in  = 0x0;
	struct context ctx = {0};
	struct context cpy = {0};

	if (!t)   return EFAULT;
	if (!key) return EFAULT;
	if (!len) return EINVAL;

	ctx.nod = (uintptr_t)t;

	do {
		if (nod_walk(&ctx, key, len)) return ENOENT;

		in = nod(ctx.nod);
		ctx.nod = in->chld[ctx.bit];
		if (!ctx.nod) return ENOENT;

		if (isnod(ctx.nod) && in->chld[0] && in->chld[1]) cpy = ctx;

	} while (isnod(ctx.nod));

	if (nod_match(&ctx, key, len)) return ENOENT;

	free(leaf(ctx.nod));
	in->chld[ctx.bit] = 0x0;

	ctx = cpy;
	nod_realign(&ctx, anc, in->chld[!ctx.bit]);

	return 0;
}

bool
set_contains(Set *t, uint8_t *key, size_t len)
{
	struct context ctx = {0};

	if (!len) return false;
	if (!t)   return -1;
	if (!key) return -1;

	ctx.nod = (uintptr_t)t;

	if (nod_traverse(&ctx, key, len) != SET_EXTERN)
		return false;
	if (nod_match(&ctx, key, len) != SET_MATCH)
		return false;

	return true;

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
