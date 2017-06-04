#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <vec.h>
#include <util.h>
#include <pat.h>
#include <pat.ih>

#define token(id, ch) ((struct token[]){ .id = id, .ch = ch })

enum {
	tok_null,
	tok_lit,
	tok_sub,
};

struct token {
	uint8_t id;
	uint8_t ch;
};

static int grow_cat(uintptr_t *, uint8_t const *);
static int grow_char(uintptr_t *, uint8_t const *);
static int grow_close(uintptr_t *, uint8_t const *);
static int grow_escape(uintptr_t *, uint8_t const *);
static int grow_open(uintptr_t *, uint8_t const *);
static uintptr_t grow_subexpr(uintptr_t *, uintptr_t);

static int shift_close(uint8_t *, uint8_t *, char const *);
static int shift_escape(uint8_t *, uint8_t *, char const *);
static int shift_liter(uint8_t *, uint8_t *, char const *);
static int shift_token(uint8_t *, uint8_t *, char const *);

static int scan(uint8_t **, char const *);
static int pat_grow(uintptr_t *, uint8_t const *);
static int pat_flush(uint8_t *, uint8_t *);
static int parse(uintptr_t *, uint8_t *);
static int pat_process(uint8_t *, uint8_t *, char const *);
static int pat_shunt(uint8_t *, uint8_t *, char const *);

static int (* const tab_shunt[])(uint8_t *, uint8_t *, char const *) = {
	['\\'] = shift_escape,
	['*']  = shift_token,
	['?']  = shift_token,
	['+']  = shift_token,
	['(']  = shift_token,
	[')']  = shift_close,
	[255]  = 0,
};

static int (* const tab_grow[])(uintptr_t *res, uint8_t const *stk) = {
	['\\'] = grow_escape,
	['_']  = grow_cat,
	['(']  = grow_open,
	[')']  = grow_close,
	[255]  = 0,
};

int
grow_cat(uintptr_t *res, uint8_t const *stk)
{
	uintptr_t cat;
	uintptr_t lef;
	uintptr_t rit;

	assert(vec_len(res) >= 2);
	vec_get(&rit, res);
	vec_get(&lef, res);

	cat = mk_cat(lef, rit);
	if (!cat) goto nomem;

	vec_put(res, &cat);

	return pat_grow(res, ++stk);

nomem:
	vec_put(res, &rit);
	vec_put(res, &lef);
	return ENOMEM;
}

int
grow_char(uintptr_t *res, uint8_t const *stk)
{
	uintptr_t nod;

	nod = mk_leaf(*stk);
	if (!nod) return ENOMEM;

	vec_put(res, &nod);

	return pat_grow(res, ++stk);
}

int
grow_close(uintptr_t *res, uint8_t const *stk)
{
	uintptr_t sub;

	sub = grow_subexpr(res, 0);
	if (!sub) return ENOMEM;

	vec_put(res, &sub);

	return pat_grow(res, ++stk);
}

int
grow_escape(uintptr_t *res, uint8_t const *stk)
{
	return grow_char(res, ++stk);
}

int
grow_open(uintptr_t *res, uint8_t const *stk)
{
	uintptr_t tmp;

	tmp = mk_open();
	if (!tmp) return ENOMEM;

	vec_put(res, &tmp);

	return pat_grow(res, ++stk);
}

int
grow_rep(uintptr_t *res, uint8_t const *stk)
{
	return -1;
}

uintptr_t
grow_subexpr(uintptr_t *res, uintptr_t rit)
{
	uintptr_t lef;
	uintptr_t acc;
	
	vec_get(&lef, res);
	acc = nod_attach(lef, rit);
	if (!acc) goto nomem;

	if (is_subexpr(acc)) return acc;
	else return grow_subexpr(res, acc);

nomem:
	vec_put(res, &lef);
	if (rit) vec_put(res, &rit);

	return ENOMEM;
}


int
shift_close(uint8_t *stk, uint8_t *aux, char const *src)
{
	return shift_token(stk, aux, src);
}

int
shift_escape(uint8_t *stk, uint8_t *aux, char const *src)
{
	++src;
	vec_put(stk, (char[]){'\\'});
	vec_put(stk, src);
	++src;
	return pat_shunt(stk, aux, src);
}

int
shift_char(uint8_t *stk, uint8_t *aux, char const *src)
{
	uint8_t ch; 

	memcpy(&ch, src, 1);
	if (tab_grow[ch]) vec_put(stk,(char[]){'\\'});

	vec_put(stk, src);

	return pat_shunt(stk, aux, ++src);
}

int
shift_concat(uint8_t *stk, uint8_t *aux, char const *src)
{
	shift_char(stk, aux, src);
	vec_put(stk,(char[]){'_'});
	return 0;
}

int
shift_liter(uint8_t *stk, uint8_t *aux, char const *src)
{
	if (vec_len(stk) == 1) return shift_char(stk, aux, src);
	else return shift_concat(stk, aux, src);
}

int
shift_token(uint8_t *stk, uint8_t *aux, char const *src)
{
	vec_put(stk, src);
	return pat_shunt(stk, aux, ++src);
}

int
pat_grow(uintptr_t *res, uint8_t const *stk)
{
	uint8_t ch;
	int (*fn)();

	if (!*stk) return 0;

	memcpy(&ch, stk, 1);

	if (!ch) return 0;

	fn = tab_grow[ch];
	if (fn) return fn(res, stk);
	else return grow_char(res, stk);
}

int
pat_flush(uint8_t *stk, uint8_t *aux)
{
	return 0;
}

int
parse(uintptr_t *dst, uint8_t *stk)
{
	uintptr_t *res;
	int err;

	res = vec_alloc(uintptr_t, vec_len(stk));
	if (!res) return ENOMEM;

	err = pat_grow(res, stk);
	if (err) goto finally;

finally:
	if (!err) *dst = res[0];
	vec_free(res);
	return err;
}

int
pat_process(uint8_t *stk, uint8_t *aux, char const *src)
{
	int err;

	vec_put(stk, (char[]){'('});
	err = pat_shunt(stk, aux, src);
	if (err) return err;
	vec_put(stk, (char[]){')'});

	return 0;
}

int
scan(uint8_t **stk, char const *src)
{
	size_t const len = strlen(src) + 1;
	uint8_t *aux = 0;
	int err;

	*stk = vec_alloc(uint8_t, len * 2);
	aux = vec_alloc(uint8_t, len);

	if (!*stk || !aux) {
		err = ENOMEM;
		goto finally;
	}

	err = pat_process(*stk, aux, src);
	if (err) goto finally;

finally:
	if (err) vec_free(*stk);
	vec_free(aux);

	return err;
}

int
pat_shunt(uint8_t *stk, uint8_t *aux, char const *src)
{
	int (*fn)();
	uint8_t ch;

	if (!*src) return pat_flush(stk, aux);

	memcpy(&ch, src, 1);

	fn = tab_shunt[ch];
	if (fn) return fn(stk, aux, src);
	else return shift_liter(stk, aux, src);
}

int
pat_parse(uintptr_t *dst, char const *src)
{
	uint8_t *stk;
	int err;

	assert(dst != 0x0);
	assert(src != 0x0);

	*dst = 0;

	err = scan(&stk, src);
	if (err) goto finally;

	err = parse(dst, stk);
	if (err) goto finally;

finally:
	return err;
}
