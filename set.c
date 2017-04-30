#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "set.h"
#include "util.h"

static size_t const SET_MAX = SIZE_MAX / 8;

struct set {
	uintptr_t root;
};

struct internal {
	size_t    crit;
	uintptr_t chld[2];
};

struct external {
	size_t  len;
	uint8_t elem[];
};

struct key {
	uint8_t const *src;
	size_t const   len;
};

static inline bool              isleaf(uintptr_t);
static inline bool              isnode(uintptr_t);
static inline struct external * leaf(uintptr_t);
static inline struct internal * node(uintptr_t);
static inline uintptr_t         tag_leaf(struct external *);
static inline uintptr_t         tag_node(struct internal *);

static uint8_t key_index(struct key *, size_t);
static uint8_t key_index_or_eol(struct key *, size_t);
static bool    key_match(struct key *, struct external *);
static bool    key_prefix(struct key *, struct external *);

static struct external * leaf_ctor(struct key *);

static size_t              nod_compare(struct external *, struct key *);
static int                 nod_init(struct internal *, struct key *, struct external *);
static int                 nod_insert(uintptr_t *, struct internal *, struct key *);
static void                nod_freetree(uintptr_t);
static uintptr_t           nod_locate(uintptr_t *, struct key *);
static bool                nod_match(struct internal *, struct key *);
static size_t              nod_popcount(uintptr_t);
static uintptr_t           nod_scan(uintptr_t, struct key *);
static struct external *   nod_traverse(uintptr_t, struct key *);
static uintptr_t *         nod_walk(uintptr_t *, struct internal *, struct key *);

static int  set_do_add(uintptr_t *, struct key *);
static int  set_do_remove(uintptr_t *, struct key *);
static void set_do_query(void ***, size_t *, size_t, uintptr_t, struct key *);

bool
isleaf(uintptr_t p)
{
	return p ? !(p & 1) : false;
}

bool
isnode(uintptr_t p)
{
	return p ? p & 1 : false;
}

struct external *
leaf(uintptr_t p)
{
	return (void *)p;
}

struct internal *
node(uintptr_t p)
{
	return (void *)(p - 1);
}

uint8_t
key_index(struct key *k, size_t i)
{
	if (i >> 3 >= k->len) return 0;
	return !!(k->src[i >> 3] & 1 << (i & 7));
}

uint8_t
key_index_or_eol(struct key *k, size_t i)
{
	if (i >> 3 >= k->len) return -1;
	return !!(k->src[i >> 3] & 1 << (i & 7));
}

bool
key_match(struct key *k, struct external *x)
{
	if (k->len != x->len) return false;

	return !memcmp(k->src, x->elem, k->len);
}

bool
key_prefix(struct key *k, struct external *x)
{
	return !memcmp(k->src, x->elem, umin(k->len, x->len));
}

uintptr_t
tag_leaf(struct external *n)
{
	return (uintptr_t)n;
}

uintptr_t
tag_node(struct internal *n)
{
	return (uintptr_t)n + 1;
}

struct external *
leaf_ctor(struct key *key)
{
	struct external *ret;

	ret = malloc(sizeof *ret + key->len);
	if (!ret) return 0x0;

	ret->len = key->len;
	memcpy(ret->elem, key->src, key->len);

	return ret;
}

size_t
nod_compare(struct external *new, struct key *key)
{
	size_t pos = 0;
	uint8_t off = 0;
	uint8_t diff = 0;

	while (new->elem[pos] == key->src[pos]) ++pos;

	diff = new->elem[pos] ^ key->src[pos];

	if (diff & 0xf0) diff >>= 4, off |= 4;
	if (diff & 0x0c) diff >>= 2, off |= 2;
	if (diff & 0x02) diff >>= 1, off |= 1;

	return (pos << 3) + off;
}

int
nod_init(struct internal *res, struct key *key, struct external *ex)
{
	struct external *new;
	uint8_t bit;

	new = leaf_ctor(key);
	if (!new) return ENOMEM;

	res->crit = nod_compare(ex, key);
	bit = key_index(key, res->crit);
	res->chld[bit] = tag_leaf(new);

	return 0;
}

int
nod_insert(uintptr_t *dest, struct internal *nod, struct key *key)
{
	uint8_t bit;

	bit = key_index(key, nod->crit);
	nod->chld[!bit] = *dest;
	*dest = tag_node(nod);

	return 0;
}

void
nod_freetree(uintptr_t cur)
{
	struct internal *nod;

	nod = node(cur);

	if (isleaf(nod->chld[0])) free((void *)nod->chld[0]);
	else nod_freetree(nod->chld[0]);

	if (isleaf(nod->chld[1])) free((void *)nod->chld[1]);
	else nod_freetree(nod->chld[1]);

	free(nod);
}

uintptr_t
nod_locate(uintptr_t *prev, struct key *key)
{
	int8_t bit = 0;
	struct internal *nod = 0x0;
	uintptr_t cur = *prev;

	if (isnode(*prev)) do {
		*prev = cur;
		if (nod) cur = nod->chld[bit];
		nod = node(cur);
		bit = key_index(key, nod->crit);
	} while (isnode(nod->chld[bit]));

	return cur;
}

bool
nod_match(struct internal *nod, struct key *key)
{
	uint8_t i;

	for (i = 0; i < 2; ++i) {
		if (isleaf(nod->chld[i])) {
			if (key_prefix(key, leaf(nod->chld[i]))) {
				return true;
			}
		} else if (nod_match(node(nod->chld[i]), key)) {
			return true;
		}
	}

	return false;
}

size_t
nod_popcount(uintptr_t cur)
{
	struct internal *nod;
	size_t ret = 0;

	if (isleaf(cur)) return 1;

	nod = node(cur);

	if (isleaf(nod->chld[0])) ++ret;
	else ret += nod_popcount(nod->chld[0]);

	if (isleaf(nod->chld[1])) ++ret;
	else ret += nod_popcount(nod->chld[1]);

	return ret;
}

uintptr_t
nod_scan(uintptr_t cur, struct key *key)
{
	int8_t bit;
	struct internal *nod;

	while (isnode(cur)) {
		nod = node(cur);
		bit = key_index_or_eol(key, nod->crit);
		if (bit == -1) return cur;
		cur = nod->chld[bit];
	}

	return cur;
}

struct external *
nod_traverse(uintptr_t cur, struct key *key)
{
	int8_t bit = 0;
	struct internal *nod;

	while (isnode(cur)) {
		nod = node(cur);
		bit = key_index(key, nod->crit);
		cur = nod->chld[bit];
	}

	return leaf(cur);
}

uintptr_t *
nod_walk(uintptr_t *dest, struct internal *nod, struct key *key)
{
	struct internal *cmp;
	int8_t bit;

	while (isnode(*dest)) {

		cmp = node(*dest);
		if (cmp->crit > nod->crit) break;

		bit = key_index(key, cmp->crit);
		dest = cmp->chld + bit;

	}

	return dest;
}

set_t *
set_alloc(void)
{
	return calloc(1, sizeof (set_t));
}

void
set_free(set_t *t)
{
	if (isleaf(t->root)) free((void *)t->root);
	else nod_freetree(t->root);
	free(t);
}

int
set_add(set_t *t, uint8_t *src, size_t len)
{
	struct key key = { .src = src, .len = len, };
	uintptr_t *dest = &t->root;
	struct external *new = 0x0;

	if (!t)   return EFAULT;
	if (!src) return EFAULT;
	if (!len) return EINVAL;
	if (len > SET_MAX) return EOVERFLOW;

	if (!t->root) {
		new = leaf_ctor(&key);
		if (!new) return ENOMEM;
		t->root = tag_leaf(new);
		return 0;
	}

	return set_do_add(dest, &key);
}

int
set_do_add(uintptr_t *dest, struct key *key)
{
	int err = 0;
	struct external *ex = 0x0;
	struct internal *nod = 0x0;

	ex = nod_traverse(*dest, key);

	nod = malloc(sizeof *nod);
	if (!nod) goto nomem;

	err = nod_init(nod, key, ex);
	if (err) goto finally;

	dest = nod_walk(dest, nod, key);

	err = nod_insert(dest, nod, key);
	if (err) goto finally;

	return 0;

nomem:
	err = ENOMEM;

finally:
	free(nod);
	return err;
}

int
set_remove(set_t *t, uint8_t *src, size_t len)
{
	struct key key = { .src = src, .len = len, };

	if (!t)   return EFAULT;
	if (!src) return EFAULT;
	if (!len) return EFAULT;
	if (len > SET_MAX) return EOVERFLOW;

	if (!t->root) return ENOENT;

	return set_do_remove(&t->root, &key);
}

int
set_do_remove(uintptr_t *dest, struct key *key)
{
	struct internal *nod = 0x0;
	struct external *ex = 0x0;
	uintptr_t loc = 0;
	uint8_t bit = 0;

	loc = nod_locate(dest, key);

	if (isleaf(*dest)) ex = leaf(*dest);
	else nod = node(*dest);

	if (isnode(loc)) nod = node(loc);

	if (!ex) ex = leaf(nod->chld[ key_index(key, nod->crit) ]);

	if (!key_match(key, ex)) return ENOENT;

	if (!nod) {
		free(ex);
		*dest = 0x0;
		return 0;
	}

	if (nod) {
		bit = nod->chld[1] == tag_leaf(ex);
		*dest = nod->chld[!bit];
	}
	free(ex);
	free(nod);
	return 0;
}

bool
set_contains(set_t *t, uint8_t *src, size_t len)
{
	struct key key = { .src = src, .len = len, };
	uintptr_t *dest = &t->root;
	struct external *ex = 0x0;

	if (!t)   return -1;
	if (!src) return -1;
	if (!len) return -1;
	if (len > SET_MAX) return false;

	if (!t->root) return false;

	ex = nod_traverse(*dest, &key);

	return key_match(&key, ex);
}

bool
set_prefix(set_t *t, uint8_t *src, size_t len)
{
	struct key key = { .src = src, .len = len, };
	uintptr_t nod = 0x0;

	if (!t)   return false;
	if (!src) return false;
	if (len > SET_MAX) false;

	nod = t->root;

	if (!nod) return false;

	nod = nod_scan(nod, &key);
	if (isleaf(nod)) return key_prefix(&key, leaf(nod));

	else return nod_match(node(nod), &key);
}

size_t
set_query(void ***res, size_t nmemb, set_t *t, uint8_t *src, size_t len)
{
	struct key key = { .src = src, .len = len, };
	size_t ind = 0;
	size_t ret = 0;

	if (!t)   return EFAULT;
	if (!src) return EFAULT;
	if (len > SET_MAX) return 0;

	if (!t->root) return 0;

	if (res && !*res) {
		ret = nod_popcount(t->root); // XXX
		*res = calloc(ret + 1, sizeof (uint8_t *));
		if (*res) nmemb = ret + 1;
	}

	set_do_query(res, &ind, nmemb, t->root, &key);

	return ind;
}

void
set_do_query(void ***res, size_t *ind, size_t nmemb, uintptr_t cur, struct key *key)
{
	uint8_t i;
	struct internal *nod;

	if (isleaf(cur) && key_prefix(key, leaf(cur))) {
		++*ind;
		if (*ind >= nmemb) return;
		if (res) (*res)[*ind - 1] = leaf(cur)->elem;
		return;
	}

	nod = node(cur);

	for (i = 0; i < 2; ++i) {
		if (isnode(nod->chld[i])) {
			set_do_query(res, ind, nmemb, nod->chld[i], key);
			continue;
		} else if (!key_prefix(key, leaf(nod->chld[i]))) continue;
		++*ind;
		if (*ind >= nmemb) continue;
		if (res) (*res)[*ind - 1] = leaf(nod->chld[i])->elem;
	}
}
