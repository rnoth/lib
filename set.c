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

enum del_ret {
	DEL_NONE = -0,
	DEL_LEAF = -1,
	DEL_NODE = -2,
};

struct context {
	uintptr_t cur;
	size_t    pos;
	uint8_t   off  : 5;
	uint8_t   shft: 3;
};

struct external {
	size_t  len;
	uint8_t bytes[];
};

struct internal {
	uint32_t  edge;
	uint8_t   crit;
	uintptr_t chld[2];
};

struct key {
	uint8_t const *src;
	size_t const   len;
	size_t         pos;
	uint8_t       shft:3;
};

static bool              isleaf(uintptr_t n);
static bool              isnode(uintptr_t n);
static uint8_t           nchld(struct internal *);
static uintptr_t         chld_of(struct internal *);
static uintptr_t         tag_leaf(struct external *);
static uintptr_t         tag_node(struct internal *);
static struct external * to_leaf(uintptr_t n);
static struct internal * to_node(uintptr_t n);

static uintptr_t  ctx_chld(struct context *, struct key *);
static uint8_t    ctx_curbit(struct context *);
static void *     ctx_curnod(struct context *);
static bool       ctx_eow(struct context *);
static void       ctx_focus(struct context *, uintptr_t);
static uintptr_t  ctx_only_chld(struct context *);
static void       ctx_step(struct context *, struct key *);

static uint8_t ext_popbit(struct context *, struct external *);

static uint8_t key_curbit(struct key *);
static uint8_t key_curbyte(struct key *);
static bool    key_eol(struct key *);
static void    key_shift(struct key *);
static void    key_step(struct key *);

static void         nod_attach(struct context *, struct key *, struct external *);
static int          nod_branch(struct context *, struct key *);
static int          nod_capture(struct context *, struct key *, uint8_t);
static int          nod_delete(struct context *, struct key *);
static int          nod_erase(struct context *, struct key *);
static void         nod_fold(struct context *);
static void         nod_init(struct external *, struct key *);
static int          nod_join(struct context *);
static int          nod_match(struct context *, struct key *);
static void         nod_pushbit(struct internal *, uint8_t);
static uint8_t      nod_popbit(struct internal *);
static int          nod_split(struct context *);
static void         nod_release(struct context *);
static int          nod_traverse(struct context *, struct key *);
static void         nod_freetree(struct internal *);
static int          nod_walk(struct context *, struct key *);

static int set_do_add(struct external *, struct context *, struct key *);
static int set_do_remove(struct context *, struct key *);

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

uint8_t
nchld(struct internal *nod)
{
	return !!nod->chld[0] + !!nod->chld[1];
}

uintptr_t 
chld_of(struct internal *nod)
{
	return nod->chld[0] + nod->chld[1];
}

struct internal *
to_node(uintptr_t n)
{
	return (void *)(n - 1);
}

uintptr_t
tag_node(struct internal *n)
{
	return n ? (uintptr_t)n + 1 : 0x0;
}

uintptr_t
tag_leaf(struct external *n)
{
	return (uintptr_t)n;
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

uintptr_t
ctx_only_chld(struct context *ctx)
{
	return chld_of(to_node(ctx->cur));
}

uint8_t
ctx_curbit(struct context *ctx)
{
	return !!(to_node(ctx->cur)->edge << ctx->off & 1 << 31);
}

void *
ctx_curnod(struct context *ctx)
{
	return isnode(ctx->cur) ? (void *)to_node(ctx->cur)
	                        : (void *)to_leaf(ctx->cur);
}

bool
ctx_eow(struct context *ctx)
{
	return ctx->off < to_node(ctx->cur)->crit;
}

void
ctx_focus(struct context *ctx, uintptr_t tmp)
{
	ctx->cur = tmp;
	ctx->off = 0;
	ctx->pos = 0;
}

void
ctx_step(struct context *ctx, struct key *key)
{
	ctx->off = 0;
	ctx->cur = to_node(ctx->cur)->chld[key_curbit(key)];
}

uint8_t
ext_popbit(struct context *ctx, struct external *ex)
{
	uint8_t ret;
	ret = ex->bytes[ctx->pos] << ctx->off;
	if (--ctx->off == 31) ctx->off = 7, --ctx->pos;
	return ret;
}

uint8_t
key_curbit(struct key *key)
{
	return key_curbyte(key) & 0x80;
}

uint8_t
key_curbyte(struct key *key)
{
	return key->src[key->pos] << key->shft;
}

bool
key_eol(struct key *key)
{
	return key->pos == key->len && key->shft == 7;
}

void
key_shift(struct key *key)
{
	++key->shft;
	if (!key->shft) ++key->pos;
}

void
key_step(struct key *key)
{
	key->shft = 0;
	++key->pos;
}

void
nod_attach(struct context *ctx, struct key *key, struct external *new)
{
	uint8_t bit = key_curbit(key);
	to_node(ctx->cur)->chld[bit] = (uintptr_t)new;
	key_shift(key);
}

int
nod_branch(struct context *ctx, struct key *key)
{

	uint8_t off = 0;

	switch (nod_traverse(ctx, key)) {
	 case SET_EXTERN: break;
	 case SET_PREFIX: return EILSEQ;
	 case SET_SLOT:   return 0;
	 case SET_SPLIT:  return nod_split(ctx);
	}

	off = key->shft;

	switch (nod_match(ctx, key)) {
	 case SET_MATCH:  return EEXIST;
	 case SET_PREFIX: return EILSEQ;
	 case SET_SPLIT:
		return nod_capture(ctx, key, off);
	}

	return -1;
}

int
nod_capture(struct context *ctx, struct key *key, uint8_t shft)
{
	uint8_t bit = 0;
	struct internal *top  = ctx_curnod(ctx);
	struct internal *bot  = 0x0;
	struct internal *new  = 0x0;
	struct internal *prev = 0x0;
	struct external *ex = to_leaf(ctx_chld(ctx, key));

	top = ctx_curnod(ctx);

	while (ctx->pos > 0 || ctx->off > shft) {

		if (!new || new->crit == 31) {
			prev = new;

			new = calloc(1, sizeof *new);
			if (!new) goto nomem;
			if (!bot) bot = new;

			if (prev) {
				bit = ext_popbit(ctx, ex);
				prev->chld[bit] = tag_node(new);
				if (!ctx->pos) continue;
			}
		}

		bit = ext_popbit(ctx, ex);
		nod_pushbit(new, bit);
	}

	bit = ext_popbit(ctx, ex);
	new->chld[bit] = tag_leaf(ex);

	return 0;

nomem:
	memset(bot->chld, 0, sizeof bot->chld);
	nod_freetree(top);
 
	return ENOMEM;
}

void
nod_fold(struct context *ctx)
{
	struct internal *br = 0x0;

	for (br = ctx_curnod(ctx);
	     nchld(br) == 1;
	     ctx_focus(ctx, tag_node(br))) {
		while (!nod_join(ctx)) ;
	}
}

void
nod_freetree(struct internal *nod)
{
	uint8_t i = 0;

	if (!nod) return;

	for (i = 0; i < 2; ++i) {
		if (isleaf(nod->chld[i])) free(to_leaf(nod->chld[i]));
		else nod_freetree(to_node(nod->chld[i]));
	}

	memset(nod, 0, sizeof *nod);
	free(nod);
}

void
nod_init(struct external *new, struct key *key)
{
	new->bytes[0] = key_curbyte(key);
	memcpy(new->bytes + 1, key->src + key->pos + 1, key->len - key->pos - 1);
	new->len = key->len - key->pos;
}

int
nod_join(struct context *ctx)
{
	uint8_t bit = 0;
	struct internal *nod  = ctx_curnod(ctx);
	struct internal *chld = to_node(chld_of(nod));

	if (nod->crit == 31) return -1;

	bit = !!nod->chld[1];
	nod_pushbit(nod, bit);

	while (umin(31 - nod->crit, chld->crit)) {

		bit = nod_popbit(chld);
		nod_pushbit(nod, bit);
		++ctx->off;
	}

	if (chld->crit) {

		bit = nod_popbit(chld);
		nod->chld[ bit] = tag_node(chld);
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
	struct external *ex = to_leaf(ctx_chld(ctx, key));

	key_shift(key);

	while (ctx->pos < ex->len && !key_eol(key)) {
		for (diff = ex->bytes[ctx->pos] ^ key_curbyte(key);
		     diff;
		     diff >>= 1, ++ctx->off) ;
		if (ctx->off) return SET_SPLIT;

		++ctx->pos, key_step(key);
	}

	return ex->len == key->len ? SET_MATCH : SET_PREFIX;
}
 
uint8_t
nod_popbit(struct internal *nod)
{
	uint8_t ret = nod->edge & 1U << 31U;
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
nod_release(struct context *ctx)
// XXX
{
	struct internal *nod = ctx_curnod(ctx);
	struct external *ex  = 0x0;
	uint8_t  bit = 0;
	uint8_t  off = 0;
	uint8_t  pos = 0;
	uint32_t rem = 0;
	uint8_t  res = 0;

	if (!isleaf(chld_of(nod))) {
		ctx_focus(ctx, chld_of(nod));
		nod_release(ctx);
		ex = ctx_curnod(ctx);
	} else ex = to_leaf(chld_of(nod));

	bit = !!nod->chld[1];
	res = bit << ctx->shft++;

	ex->bytes[0] >>= ctx->shft;
	ex->bytes[0] |= res;

	memmove(ex->bytes + nod->crit / 8,
		ex->bytes,
		ex->len - nod->crit / 8);

	pos = nod->crit / 8 + !!(nod->crit % 8);

	if (pos) do {

		res = 0;

		off = umin(nod->crit - ctx->shft, nod->crit);

		if (wh) res = nod->edge >> 32U - off;
		nod->edge  = nod->edge & ~0 << off;
		nod->crit -= off;
		ctx->shft  = nod->crit;

		ex->bytes[pos] >>= off;
		ex->bytes[pos]  |= res;

	 } while (pos --> 0);

	ctx_focus(ctx, tag_leaf(ex));

	free(nod);
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

int
nod_traverse(struct context *ctx, struct key *key)
{
	int res = 0;
	uintptr_t chld = 0;

	while (!key_eol(key)) {

		res = nod_walk(ctx, key);
		if (res) return res;

		chld = ctx_chld(ctx, key);
		if (!chld) return SET_SLOT;

		if (isleaf(chld)) return SET_EXTERN;
		ctx_focus(ctx, chld);
		key_shift(key);
	}

	return SET_PREFIX;
}

int
nod_walk(struct context *ctx, struct key *key)
{
	while (!ctx_eow(ctx)) {

		if (key_eol(key)) return SET_PREFIX;

		if (key_curbit(key) != ctx_curbit(ctx))
			return SET_SPLIT;

		key_shift(key), ++ctx->off;
	}

	return 0;
}

struct internal *
set_alloc(void)
{
	struct internal *ret;
	ret = calloc(1, sizeof *ret);
	return ret;
}

void
set_free(Set *t)
{
	nod_freetree(to_node((uintptr_t)t));
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
	if (!new) return ENOMEM;

	ctx.cur = tag_node(t);

	err = set_do_add(new, &ctx, &key);
	if (err) free(new);
	return err;
}


int
set_do_add(struct external *new, struct context *ctx, struct key *key)
{
	int err = 0;

	err = nod_branch(ctx, key);
	if (err) return err;

	nod_attach(ctx, key, new);
	nod_init(new, key);

	return 0;
}

int
set_remove(Set *t, uint8_t *src, size_t len)
{
	struct context ctx = {0};
	struct key key = { .src = src, .len = len };

	if (!t)   return EFAULT;
	if (!src) return EFAULT;
	if (!len) return EINVAL;

	ctx.cur = tag_node(t);

	return set_do_remove(&ctx, &key);
}

int
nod_delete(struct context *ctx, struct key *key)
{
	int ret = 0;
	uint8_t shft = 0;
	uintptr_t br = 0x0;

	do {
		if (nchld(ctx_curnod(ctx)) == 2) br = ctx->cur;
		if (nod_walk(ctx, key)) return ENOENT;

	} while (isleaf(ctx_chld(ctx, key))
		&& (key_shift(key), ctx_step(ctx, key), true));

	shft = key->shft;
	if (nod_erase(ctx, key)) return ENOENT;

	if (isleaf(ctx_only_chld(ctx))) ret = DEL_LEAF;
	else if (isnode(ctx_only_chld(ctx))) ret = DEL_NODE;
	else ret = DEL_NONE;

	ctx_focus(ctx, br);
	ctx->shft = shft;

	return ret;
}

int
nod_erase(struct context *ctx, struct key *key)
{
	uint8_t bit = key_curbit(key);
	if (nod_match(ctx, key)) return ENOENT;

	ctx_chld(ctx, key);
	free(to_leaf(ctx_chld(ctx, key)));
	to_node(ctx->cur)->chld[bit] = 0x0;
}

int
set_do_remove(struct context *ctx, struct key *key)
{
	switch (nod_delete(ctx, key)) {
	 case ENOENT: return ENOENT;
	 case DEL_NONE: return 0;
	 case DEL_LEAF: nod_release(ctx); break;
	 case DEL_NODE: nod_fold(ctx); break;
	}
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

	ctx.cur = tag_node(t);

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
