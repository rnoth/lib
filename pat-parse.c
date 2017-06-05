#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <vec.h>
#include <util.h>
#include <pat.h>
#include <pat.ih>

#define token(...) tok(__VA_ARGS__, 0, 0)
#define tok(i, c, ...) ((struct token[]){{ .id = i, .ch = c }})
#define peek(a) ((a + vec_len(a)))

struct token {
	uint8_t id;
	uint8_t ch;
};

static int parse_alt(uintptr_t *, struct token const *);
static int parse_cat(uintptr_t *, struct token const *);
static int parse_char(uintptr_t *, struct token const *);
static int parse_close(uintptr_t *, struct token const *);
static int parse_eol(uintptr_t *);
static int parse_nop(uintptr_t *, struct token const *);
static int parse_rep(uintptr_t *, struct token const *);

static int shunt(struct token *, struct token *, char const *);
static int shunt_alt(struct token *, struct token *, char const *);
static int shunt_char(struct token *, struct token *, char const *);
static int shunt_close(struct token *, struct token *, char const *);
static int shunt_eol(struct token *, struct token *);
static int shunt_escape(struct token *, struct token *, char const *);
static int shunt_open(struct token *, struct token *, char const *);
static int shunt_monad(struct token *, struct token *, char const *);
static int shunt_string(struct token *, struct token *, char const *);

static uint8_t oper(char const *);
static int parse(uintptr_t *, struct token const *);
static int scan(struct token **, char const *);

static int (* const tab_shunt[])() = {
	['\0'] = shunt_eol,
	['\\'] = shunt_escape,
	['*']  = shunt_monad,
	['?']  = shunt_monad,
	['+']  = shunt_monad,
	['|']  = shunt_alt,
	['(']  = shunt_open,
	[')']  = shunt_close,
	[255]  = 0,
};

static int (* const tab_grow[])() = {
	[type_nil] = parse_eol,
	[type_lit] = parse_char,
	[type_cat] = parse_cat,
	[type_opt] = parse_rep,
	[type_kln] = parse_rep,
	[type_rep] = parse_rep,
	[type_sub] = parse_close,
	[type_nop] = parse_nop,
	[type_alt] = parse_alt,
};

int
parse_alt(uintptr_t *res, struct token const *stk)
{
	uintptr_t lef;
	uintptr_t rit;
	uintptr_t alt;

	vec_get(&rit, res);
	vec_get(&lef, res);

	alt = mk_alt(lef, rit);
	if (!alt) goto nomem;

	vec_put(res, &alt);

	++stk;
	return tab_grow[stk->id](res, stk);

nomem:
	vec_put(res, &lef);
	vec_put(res, &rit);

	return ENOMEM;
}

int
parse_cat(uintptr_t *res, struct token const *stk)
{
	uintptr_t cat;
	uintptr_t lef;
	uintptr_t rit;

	vec_get(&rit, res);
	vec_get(&lef, res);

	cat = mk_cat(lef, rit);
	if (!cat) goto nomem;

	vec_put(res, &cat);

	++stk;
	return tab_grow[stk->id](res, stk);

nomem:
	vec_put(res, &rit);
	vec_put(res, &lef);
	return ENOMEM;
}

int
parse_char(uintptr_t *res, struct token const *stk)
{
	uintptr_t nod;

	nod = mk_leaf(stk->ch);
	if (!nod) return ENOMEM;

	vec_put(res, &nod);

	++stk;
	return tab_grow[stk->id](res, stk);
}

int
parse_close(uintptr_t *res, struct token const *stk)
{
	uintptr_t chld;
	uintptr_t sub;

	vec_get(&chld, res);

	sub = mk_subexpr(chld);
	if (!sub) goto nomem;

	vec_put(res, &sub);

	++stk;
	return tab_grow[stk->id](res, stk);

nomem:
	vec_put(res, &chld);
	return ENOMEM;
}

int
parse_eol(uintptr_t *res)
{
	uintptr_t chld;
	uintptr_t root;

	vec_get(&chld, res);

	root = mk_subexpr(chld);
	if (!root) goto nomem;

	vec_put(res, &root);

	return 0;

nomem:
	vec_put(res, &chld);
	return ENOMEM;
}

int
parse_nop(uintptr_t *res, struct token const *stk)
{
	++stk;
	return tab_grow[stk->id](res, stk);
}

int
parse_rep(uintptr_t *res, struct token const *stk)
{
	uintptr_t rep;
	uintptr_t chld;

	if (!vec_len(res)) return PAT_ERR_BADREP;
	vec_get(&chld, res);

	rep = mk_rep(stk->id, chld);
	if (!rep) return ENOMEM;

	vec_put(res, &rep);

	++stk;
	return tab_grow[stk->id](res, stk);
}

int
shunt(struct token *stk, struct token *aux, char const *src)
{
	int (*shift)();
	uint8_t ch;

	memcpy(&ch, src, 1);

	shift = tab_shunt[ch];
	if (shift) return shift(stk, aux, src);
	else return shunt_char(stk, aux, src);
}

int
shunt_alt(struct token *stk, struct token *aux, char const *src)
{
	vec_cat(stk, aux);
	vec_zero(aux);

	vec_put(aux, token(type_alt));
	vec_put(stk, token(type_nop));

	return shunt_char(stk, aux, ++src);
}

int
shunt_char(struct token *stk, struct token *aux, char const *src)
{
	if (vec_len(stk)) vec_put(aux, token(type_cat));

	vec_put(stk, token(type_lit, *src));
	return shunt_string(stk, aux, ++src);
}

int
shunt_close(struct token *stk, struct token *aux, char const *src)
{
	if (aux->id != type_nop) return PAT_ERR_BADPAREN; // XXX
	vec_cat(stk, aux);
	vec_zero(aux);

	return shunt_char(stk, aux, ++src);
}

int
shunt_eol(struct token *stk, struct token *aux)
{
	vec_cat(stk, aux);
	vec_zero(aux);
	return 0;
}

int
shunt_escape(struct token *stk, struct token *aux, char const *src)
{
	return shunt_char(stk, aux, ++src);
}

int
shunt_open(struct token *stk, struct token *aux, char const *src)
{
	vec_cat(stk, aux);
	vec_zero(aux);

	vec_put(aux, token(type_nop));

	return shunt(stk, aux, ++src);
}

int
shunt_monad(struct token *stk, struct token *aux, char const *src)
{
	uint8_t op = oper(src);
	vec_put(stk, token(op, *src));
	return shunt(stk, aux, ++src);
}

int
shunt_string(struct token *stk, struct token *aux, char const *src)
{
	return shunt(stk, aux, src);
}

uint8_t
oper(char const *src)
{
	uint8_t const tab[] = {
		['?'] = type_opt,
		['*'] = type_kln,
		['+'] = type_rep,
		['('] = type_sub,
	};
	uint8_t ch;

	memcpy(&ch, src, 1);

	return tab[ch];
}

int
parse(uintptr_t *dst, struct token const *stk)
{
	uintptr_t *res;
	int err;

	res = vec_alloc(uintptr_t, vec_len(stk));
	if (!res) return ENOMEM;

	err = tab_grow[stk->id](res, stk);
	if (err) goto finally;

finally:
	if (!err) *dst = res[0];
	vec_free(res);
	return err;
}

int
scan(struct token **stk, char const *src)
{
	size_t const len = strlen(src) + 1;
	struct token *aux = 0;
	int err;

	*stk = vec_alloc(struct token, len * 2);
	 aux = vec_alloc(struct token, len);

	if (!*stk || !aux) {
		err = ENOMEM;
		goto finally;
	}

	err = shunt(*stk, aux, src);
	if (err) goto finally;

finally:
	if (err) vec_free(*stk);
	vec_free(aux);

	return err;
}

int
pat_parse(uintptr_t *dst, char const *src)
{
	struct token *stk;
	int err;

	err = scan(&stk, src);
	if (err) return err;

	err = parse(dst, stk);
	if (err) return err;

	return 0;
}
