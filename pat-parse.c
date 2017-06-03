#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <vec.h>
#include <util.h>
#include <pat.h>
#include <pat.ih>

static int grow_cat(uintptr_t *, uint8_t const *);
static int grow_char(uintptr_t *, uint8_t const *);
static int grow_close(uintptr_t *, uint8_t const *);
static int grow_escape(uintptr_t *, uint8_t const *);
static int grow_open(uintptr_t *, uint8_t const *);

static int shift_close(uint8_t *, uint8_t *, char const *);
static int shift_escape(uint8_t *, uint8_t *, char const *);
static int shift_liter(uint8_t *, uint8_t *, char const *);
static int shift_token(uint8_t *, uint8_t *, char const *);

static int pat_grow(uintptr_t *, uint8_t const *);
static int pat_flush(uint8_t *, uint8_t *);
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

	vec_get(&rit, res);
	vec_get(&lef, res);

	cat = mk_cat(lef, rit);
	if (!cat) {
		vec_put(res, &lef);
		vec_put(res, &rit);
		return ENOMEM;
	}

	vec_put(res, &cat);

	return pat_grow(res, ++stk);
}

int
grow_char(uintptr_t *res, uint8_t const *stk)
{
	uint8_t *ch;

	ch = malloc(sizeof *ch);
	if (!ch) return ENOMEM;

	memcpy(ch, stk, sizeof *ch);

	vec_put(res, (uintptr_t[]){tag_leaf(ch)});

	return pat_grow(res, ++stk);
}

int
grow_close(uintptr_t *res, uint8_t const *stk)
{
	uintptr_t lef = 0;
	uintptr_t rit = 0;

again:
	vec_get(&lef, res);
	if (!is_open(lef)) {
		rit = nod_attach(lef, rit);
		if (!rit) goto nomem;
		goto again;
	}

	nod_attach(lef, rit);

	vec_put(res, &lef);

	return pat_grow(res, ++stk);
nomem:
	if (lef) vec_put(res, &lef);
	if (rit) vec_put(res, &rit);
	return ENOMEM;
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
shift_liter(uint8_t *stk, uint8_t *aux, char const *src)
{
	uint8_t ch; 

	memcpy(&ch, src, 1);
	if (tab_shunt[ch]) {
		vec_put(stk, (char[]){'\\'});
	}

	vec_put(stk, src);

	++src;
	return pat_shunt(stk, aux, src);
}

int
shift_token(uint8_t *stk, uint8_t *aux, char const *src)
{
	vec_put(aux, src);
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
	size_t const len = strlen(src);
	uint8_t *stk = 0;
	uint8_t *aux = 0;
	uintptr_t *res = 0;
	int err;

	assert(dst != 0x0);
	assert(src != 0x0);

	*dst = 0;

	stk = vec_alloc(uint8_t, len * 2 + 2);
	aux = vec_alloc(uint8_t, len * 2);
	res = vec_alloc(uintptr_t, len + 2);

	if (!aux || !stk || !res) {
		err = ENOMEM;
		goto finally;
	}

	err = pat_process(stk, aux, src);
	if (err) goto finally;

	assert(vec_len(aux) == 0);

	err = pat_grow(res, stk);
	if (err) goto finally;

	*dst = res[0];

finally:
	vec_free(stk);
	//vec_free(aux);
	vec_free(res);

	return err;
}
