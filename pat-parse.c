#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <arr.h>
#include <util.h>
#include <pat.h>
#include <pat.ih>

#define token(...) tok(__VA_ARGS__, 0, 0)
#define tok(i, c, ...) ((struct token[]){{ .id = i, .ch = c }})

#define prec(tok) (tab_prec[(tok)->id])

enum state {
	st_init,
	st_str,
};

struct token {
	uint8_t id;
	uint8_t ch;
};

static int parse(uintptr_t *, struct token const *);
static int parse_alt(uintptr_t *, struct token const *);
static int parse_cat(uintptr_t *, struct token const *);
static int parse_char(uintptr_t *, struct token const *);
static int parse_close(uintptr_t *, struct token const *);
static int parse_eol(uintptr_t *);
static int parse_nop(uintptr_t *, struct token const *);
static int parse_rep(uintptr_t *, struct token const *);

static int scan(struct token **, void const *);
static int shunt_next(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_alt(struct token *, struct token *, uint8_t const *);
static int shunt_char(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_close(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_eol(struct token *, struct token *);
static int shunt_esc(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_open(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_mon(struct token *, struct token *, uint8_t const *);

static uint8_t oper(uint8_t const *);

static int8_t const tab_prec[] = {
	[type_sub] = 0,
	[type_nop] = 0,
	[type_alt] = 1,
	[type_cat] = 2,
	[255] = 0,
};

static int (* const tab_shunt[])() = {
	['\0'] = shunt_eol,
	['\\'] = shunt_esc,
	['*']  = shunt_mon,
	['?']  = shunt_mon,
	['+']  = shunt_mon,
	['|']  = shunt_alt,
	['(']  = shunt_open,
	[')']  = shunt_close,
	[255]  = 0,
};

static int (* const tab_parse[])() = {
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

void
tok_pop_greater(struct token *stk, struct token *op, int8_t pr)
{
	struct token *top;

	if (arr_len(op) == 0) return;

	top = arr_peek(op);

	if (prec(top) < pr) return;

	arr_put(stk, top);
	arr_rm(op, arr_len(op)-1);

	tok_pop_greater(stk, op, pr);
}

int
parse(uintptr_t *dst, struct token const *stk)
{
	uintptr_t *res;
	int err;

	res = arr_alloc(sizeof *res, arr_len(stk));
	if (!res) return ENOMEM;

	err = tab_parse[stk->id](res, stk);
	if (err) goto finally;

finally:
	if (!err) *dst = res[0];
	arr_free(res);
	return err;
}

int
parse_alt(uintptr_t *res, struct token const *stk)
{
	uintptr_t lef;
	uintptr_t rit;
	uintptr_t alt;

	arr_get(&rit, res);
	arr_get(&lef, res);

	alt = mk_alt(lef, rit);
	if (!alt) goto nomem;

	arr_put(res, &alt);

	++stk;
	return tab_parse[stk->id](res, stk);

nomem:
	arr_put(res, &lef);
	arr_put(res, &rit);

	return ENOMEM;
}

int
parse_cat(uintptr_t *res, struct token const *stk)
{
	uintptr_t cat;
	uintptr_t lef;
	uintptr_t rit;

	arr_get(&rit, res);
	arr_get(&lef, res);

	cat = mk_cat(lef, rit);
	if (!cat) goto nomem;

	arr_put(res, &cat);

	++stk;
	return tab_parse[stk->id](res, stk);

nomem:
	arr_put(res, &rit);
	arr_put(res, &lef);
	return ENOMEM;
}

int
parse_char(uintptr_t *res, struct token const *stk)
{
	uintptr_t nod;

	nod = mk_leaf(stk->ch);
	if (!nod) return ENOMEM;

	arr_put(res, &nod);

	++stk;
	return tab_parse[stk->id](res, stk);
}

int
parse_close(uintptr_t *res, struct token const *stk)
{
	uintptr_t chld;
	uintptr_t sub;

	arr_get(&chld, res);

	sub = mk_subexpr(chld);
	if (!sub) goto nomem;

	arr_put(res, &sub);

	++stk;
	return tab_parse[stk->id](res, stk);

nomem:
	arr_put(res, &chld);
	return ENOMEM;
}

int
parse_eol(uintptr_t *res)
{
	uintptr_t chld;
	uintptr_t root;

	arr_get(&chld, res);

	root = mk_subexpr(chld);
	if (!root) goto nomem;

	arr_put(res, &root);

	return 0;

nomem:
	arr_put(res, &chld);
	return ENOMEM;
}

int
parse_nop(uintptr_t *res, struct token const *stk)
{
	++stk;
	return tab_parse[stk->id](res, stk);
}

int
parse_rep(uintptr_t *res, struct token const *stk)
{
	uintptr_t rep;
	uintptr_t chld;

	if (!arr_len(res)) return PAT_ERR_BADREP;
	arr_get(&chld, res);

	rep = mk_rep(stk->id, chld);
	if (!rep) return ENOMEM;

	arr_put(res, &rep);

	++stk;
	return tab_parse[stk->id](res, stk);
}

int
scan(struct token **stk, void const *src)
{
	size_t const len = strlen(src) + 1;
	struct token *op = 0;
	int err;

	*stk = arr_alloc(sizeof **stk, len * 2);
	 op = arr_alloc(sizeof *op, len);

	if (!*stk || !op) {
		err = ENOMEM;
		goto finally;
	}

	err = shunt_next(*stk, op, src, st_init);
	if (err) goto finally;

finally:
	if (err) arr_free(*stk);
	arr_free(op);

	return err;
}

int
shunt_next(struct token *stk, struct token *op, uint8_t const *src, enum state st)
{
	int (*sh)();

	sh = tab_shunt[*src];
	if (sh) return sh(stk, op, src, st);
	else return shunt_char(stk, op, src, st);
}

int
shunt_alt(struct token *stk, struct token *op, uint8_t const *src)
{
	tok_pop_greater(stk, op, tab_prec[type_alt]);
	arr_put(op, token(type_alt));
	return shunt_next(stk, op, ++src, st_init);
}

int
shunt_char(struct token *stk, struct token *op, uint8_t const *src, enum state st)
{
	arr_put(stk, token(type_lit, *src));
	if (st == st_str) arr_put(op, token(type_cat, '_'));
	return shunt_next(stk, op, ++src, st_str);
}

int
shunt_close(struct token *stk, struct token *op, uint8_t const *src, enum state st)
{
	struct token tok[1];
	if (!arr_len(op)) return PAT_ERR_BADPAREN;

	arr_get(tok, op);

	if (tok->id == type_nop) {
		arr_put(stk, token(type_sub));
		if (tok->ch == st_str) arr_put(op, token(type_cat, '_'));
		return shunt_next(stk, op, ++src, st_str);
	}

	arr_put(stk, tok);
	return shunt_close(stk, op, src, st);
}

int
shunt_eol(struct token *stk, struct token *op)
{
	struct token tok[1];

	if (!arr_len(op)) return 0;

	arr_get(tok, op);
	arr_put(stk, tok);
	return shunt_eol(stk, op);
}

int
shunt_esc(struct token *stk, struct token *op, uint8_t const *src, enum state st)
{
	return shunt_char(stk, op, ++src, st);
}

int
shunt_open(struct token *stk, struct token *op, uint8_t const *src, enum state st)
{
	arr_put(op, token(type_nop, st));
	return shunt_next(stk, op, ++src, st_init);
}

int
shunt_mon(struct token *stk, struct token *op, uint8_t const *src)
{
	uint8_t ty = oper(src);
	arr_put(stk, token(ty, *src));
	return shunt_next(stk, op, ++src, st_str);
}

uint8_t
oper(uint8_t const *src)
{
	uint8_t const tab[] = {
		['?'] = type_opt,
		['*'] = type_kln,
		['+'] = type_rep,
		['('] = type_sub,
	};
	return tab[*src];
}

int
pat_parse(uintptr_t *dst, void const *src)
{
	struct token *stk;
	int err;

	err = scan(&stk, src);
	if (err) return err;

	err = parse(dst, stk);
	if (err) return err;

	return 0;
}
