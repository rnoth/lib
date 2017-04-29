#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "set.h"
#include "util.h"

enum set_ret {
	SET_MATCH  = -0,
	SET_SPLIT  = -1,
	SET_SLOT   = -2,
	SET_EXTERN = -3,
	SET_PREFIX = -4,
};

struct context {
	uintptr_t cur;
	size_t    pos;
	uint8_t   off  : 5;
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

struct key {
	uint8_t const *src;
	size_t const   len;
	size_t         pos;
	uint8_t       shft:3;
};

static int          nod_join(struct context *, struct internal *);
static void         nod_pushbit(struct internal *, uint8_t);
static uint8_t      nod_popbit(struct internal *);
static void         nod_release(struct context *, struct internal *);
static void         nod_tree_free(struct internal *);
static enum set_ret nod_walk(struct context *, struct key *);

static inline bool              isleaf(uintptr_t n);
static inline bool              isnode(uintptr_t n);
static inline bool              key_eol(struct key *);
static inline uint8_t           key_curbyte(struct key *);
static inline struct internal * to_node(uintptr_t n);
static inline struct external * to_leaf(uintptr_t n);

bool
isleaf(uintptr_t n)
{
	return n ? n ^ 1 : false;
}

bool
isnode(uintptr_t n)
{
	return n ? n & 1 : false;
}

struct internal *
to_node(uintptr_t n)
{
	return (void *)(n - 1);
}

struct external *
to_leaf(uintptr_t n)
{
	return (void *)n;
}

uintptr_t
ctx_chld(struct context *ctx, struct key *key)
{
	return to_node(ctx->cur)->chld[key_curbyte(key) & 0x80];
}

void
ctx_focus(struct context *ctx, uintptr_t tmp)
{
	ctx->cur = tmp;
	ctx->off = 0;
	ctx->pos = 0;
}

uint8_t
key_curbyte(struct key *key)
{
	return key->src[key->pos];
}

bool
key_eol(struct key *key)
{
	return key->pos < key->len || key->shft != 7;
}

uint8_t
key_step(struct key *key)
{
	++key->shft;
	if (!key->shft) ++key->pos;
	return key_curbyte(key) & 0x80;
}

uint8_t
nod_popbit(struct internal *nod)
{
	uint8_t ret = nod->edge & 1 << 31;
	nod->edge <<= 1;
	--nod->crit;
	return ret;
}

void
nod_pushbit(struct internal *nod, uint8_t bit)
{
	nod->edge |= bit << nod->crit++;
}

void
nod_attach(struct context *ctx, struct external *new)
{
	uint8_t bit = !!to_node(ctx->cur)->chld[1];
	to_node(ctx->cur)->chld[!bit] = (uintptr_t)new;
}

void
nod_align_internal(struct context *ctx, struct internal *br)
{ 
	while (!br->chld[0] != !br->chld[1]) {
		if (nod_join(ctx, br)) {
			br = to_node(br->chld[0] + br->chld[1]);
		}
	}
}

void
nod_align_external(struct context *ctx, struct internal *br)
{
	while (isnode(br->chld[0] + br->chld[1])) {
		if (nod_join(ctx, br)) {
			br = to_node(br->chld[0] + br->chld[1]);
		}
	}

	nod_release(ctx, br);
}

void
nod_realign(struct context *ctx, struct internal *anc, uintptr_t rem)
{
	if (isnode(rem)) nod_align_internal(ctx, anc);
	else if (isleaf(rem)) nod_align_external(ctx, anc);
}

int
nod_capture(struct context *ctx)
{
	return -1;
}

void
nod_init(struct external *new, struct context *ctx, struct key *key)
{
	new->bytes[0] = key_curbyte(key);
	memcpy(new->bytes + 1, key + ctx->pos + 1, key->len - ctx->pos - 1);
	new->len = key->len - ctx->pos;
}

int
nod_join(struct context *ctx, struct internal *nod)
{
	uint8_t bit  = 0;
	struct internal *chld = to_node(nod->chld[0] + nod->chld[1]);

	if (nod->crit == 31) return -1;

	bit = !!nod->chld[1];
	nod_pushbit(nod, bit);

	while (umin(31 - nod->crit, chld->crit)) {

		bit = nod_popbit(chld);
		if (!chld->crit) break;

		nod_pushbit(nod, bit);
		++ctx->off;
	}

	if (chld->crit) {

		bit = nod_popbit(chld);
		nod->chld[ bit] = (uintptr_t)chld + 1;
		nod->chld[!bit] = 0x0;
		return -1;

	} else {

		memcpy(nod->chld, chld->chld, sizeof nod->chld);
		free(chld);

		return 0;
	}
}

enum set_ret
nod_match(struct context *ctx, struct key *key)
{
	uint8_t diff = 0;
	size_t ind = 0;
	struct external *ex = to_leaf(ctx->cur);

	while (ctx->pos < ex->len && !key_eol(key)) {
		for (diff = ex->bytes[ind] ^ key_curbyte(key);
		     diff;
		     diff >>= 1, ++ctx->off) ;
		if (ctx->off) return SET_SPLIT;

		++ctx->pos, ++key->pos;
	}

	return ex->len == key->len ? SET_MATCH : SET_PREFIX;
}
 
void
nod_release(struct context *ctx, struct internal *nod)
{
}
int
nod_split(struct context *ctx)
{
	struct internal *par = 0x0;
	struct internal *chld = 0x0;
	uint8_t bit = 0;

	par   = to_node(ctx->cur);
	chld = calloc(1, sizeof *chld);

	if (!chld) return ENOMEM;

	bit = par->edge << ctx->off & 0x80;
	chld->edge = par->edge << ctx->off + 1;

	if (ctx->off) par->edge &= ~0 << 32 - ctx->off;
	else par->edge = 0;

	par->crit = ctx->off;
	memcpy(chld->chld, par->chld, sizeof chld->chld);

	par->chld[ bit] = (uintptr_t)chld + 1;
	par->chld[!bit] = 0x0;

	return 0;
}

enum set_ret
nod_traverse(struct context *ctx, struct key *key)
{
	int res = 0;
	uintptr_t tmp = 0;

	while (!key_eol(key)) {

		res = nod_walk(ctx, key);
		if (res) return res;

		tmp = ctx_chld(ctx, key);
		if (!tmp) return SET_SLOT;

		key_step(key);

		ctx_focus(ctx, tmp);
		if (isleaf(tmp)) return SET_EXTERN;
	}

	return SET_PREFIX;
}

void
nod_tree_free(struct internal *nod)
{
	uint8_t i = 0;

	if (!nod) return;

	for (i = 0; i < 2; ++i) {
		if (isleaf(nod->chld[i])) free(to_leaf(nod->chld[i]));
		else nod_tree_free(to_node(nod->chld[i]));
	}

	memset(nod, 0, sizeof *nod);
	free(nod);
}

enum set_ret
nod_walk(struct context *ctx, struct key *key)
{
	uint8_t  crit = 0;
	uint32_t edge = 0;
	struct internal *nod = to_node(ctx->cur);

	edge = nod->edge;

	while (ctx->off < nod->crit) {

		if (key_eol(key)) return SET_PREFIX;

		if (key_curbyte(key) & 0x80 != edge & 0x80) {
			ctx->off = nod->crit - crit + 1;
			return SET_SPLIT;
		}

		key_step(key), ++ctx->off, edge <<= 1;
	}

	return 0;
}

struct internal *
set_alloc(void)
{
	struct internal *ret;
	ret = calloc(1, sizeof *ret);
	ret = (void *)((char *)ret + 1);
	return ret;
}

void
set_free(Set *t)
{
	nod_tree_free(to_node((uintptr_t)t));
}

int
set_add(Set *t, uint8_t *src, size_t len)
{
	int err = 0;
	struct external *new = 0x0;
	struct context ctx = {0};
	struct key key = { .src = src, .len = len };

	if (!t)   return EFAULT;
	if (!src) return EFAULT;
	if (!len) return EINVAL;

	new = calloc(1, sizeof *new + len);
	if (!new) goto finally;

	ctx.cur = (uintptr_t)t;

	switch (nod_traverse(&ctx, &key)) {
	case SET_PREFIX:
		err = EILSEQ;
		goto finally;

	case SET_SLOT:
		break;

	case SET_EXTERN: break;
		switch (nod_match(&ctx, &key)) {
		case SET_MATCH:  return EEXIST;
		case SET_PREFIX: return EILSEQ;
		case SET_SPLIT:
			err = nod_capture(&ctx);
			if (err) goto finally;
		}
		break;

	case SET_SPLIT:
		err = nod_split(&ctx);
		if (err) goto finally;
		break;
	}

	nod_init(new, &ctx, &key);
	nod_attach(&ctx, new);

finally:
	if (err) free(new);
	return err;
}

int
set_remove(Set *t, uint8_t *src, size_t len)
{
	//struct internal *anc = 0x0;
	struct internal *nod = 0x0;
	struct context ctx = {0};
	struct context cpy = {0};
	struct key key = { .src = src, .len = len };

	if (!t)   return EFAULT;
	if (!src) return EFAULT;
	if (!len) return EINVAL;

	ctx.cur = (uintptr_t)t;

	do {
		if (nod_walk(&ctx, &key)) return ENOENT;

		nod = to_node(ctx.cur);
		ctx.cur = ctx_chld(&ctx, &key);
		if (!ctx.cur) return ENOENT;

		if (isnode(ctx.cur) && nod->chld[0] && nod->chld[1]) cpy = ctx;

	} while (isnode(ctx.cur));

	if (nod_match(&ctx, &key)) return ENOENT;

	free(to_leaf(ctx.cur));
	//nod->chld[ctx.bit] = 0x0;

	ctx = cpy;
	//nod_realign(&ctx, anc, nod->chld[!ctx.bit]);

	return 0;
}

bool
set_contains(Set *t, uint8_t *src, size_t len)
{
	struct context ctx = {0};
	struct key key = { .src = src, .len = len };

	if (!t)   return -1;
	if (!src) return -1;
	if (!len) return false;

	ctx.cur = (uintptr_t)t;

	if (nod_traverse(&ctx, &key) != SET_EXTERN)
		return false;
	if (nod_match(&ctx, &key) != SET_MATCH)
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
