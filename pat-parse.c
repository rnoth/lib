#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <arr.h>
#include <util.h>
#include <pat.h>
#include <pat.ih>

static int parse_next(uintptr_t *, uintptr_t, struct token const *);
static int parse_alt(uintptr_t *, uintptr_t, struct token const *);
static int parse_cat(uintptr_t *, uintptr_t, struct token const *);
static int parse_char(uintptr_t *, uintptr_t, struct token const *);
static int parse_close(uintptr_t *, uintptr_t, struct token const *);
static int parse_class(uintptr_t *, uintptr_t, struct token const *);
static int parse_eol(uintptr_t *, uintptr_t);
static int parse_rep(uintptr_t *, uintptr_t, struct token const *);

static int (* const tab_parse[])() = {
	[type_nil] = parse_eol,
	[type_lit] = parse_char,
	[type_cat] = parse_cat,
	[type_opt] = parse_rep,
	[type_kln] = parse_rep,
	[type_rep] = parse_rep,
	[type_sub] = parse_close,
	[type_nop] = parse_next,
	[type_alt] = parse_alt,
	[type_cls] = parse_class,
};

int
pat_parse(uintptr_t *dst, struct token const *stk)
{
	uintptr_t *res;
	int err;

	res = arr_alloc(sizeof *res, arr_len(stk));
	if (!res) return ENOMEM;

	err = parse_next(res, 0x0, stk);
	if (err) goto finally;

finally:
	if (!err) *dst = res[0];
	arr_free(res);
	return err;
}

int
parse_next(uintptr_t *res, uintptr_t nod, struct token const *stk)
{
	if (nod || (nod = nod_alloc()));
	else return ENOMEM;

	return tab_parse[stk->op](res, nod, stk);
}

int
parse_alt(uintptr_t *res, uintptr_t nod, struct token const *stk)
{
	uintptr_t lef;
	uintptr_t rit;

	arr_get(&rit, res);
	arr_get(&lef, res);

	nod_init(nod, type_alt, chld(lef, rit));

	arr_put(res, &nod);

	return parse_next(res, 0x0, ++stk);
}

int
parse_cat(uintptr_t *res, uintptr_t nod, struct token const *stk)
{
	uintptr_t lef;
	uintptr_t rit;

	arr_get(&rit, res);
	arr_get(&lef, res);
	nod_init(nod, type_cat, chld(lef, rit));
	arr_put(res, &nod);

	return parse_next(res, 0x0, ++stk);;
}

int
parse_char(uintptr_t *res, uintptr_t nod, struct token const *stk)
{
	arr_put(res, (uintptr_t[]){tag_leaf(&stk->ch)});
	return parse_next(res, nod, ++stk);
}

int
parse_close(uintptr_t *res, uintptr_t nod, struct token const *stk)
{
	uintptr_t chld;

	arr_get(&chld, res);
	nod_init(nod, type_sub, chld(chld, 0x0));
	arr_put(res, &nod);

	return parse_next(res, 0x0, ++stk);
}

int
parse_class(uintptr_t *res, uintptr_t nod, struct token const *stk)
{
	nod_init(nod, type_cls, chld(tag_leaf(&stk->ch),0x0));
	arr_put(res, &nod);
	return parse_next(res, 0x0, ++stk);
}

int
parse_eol(uintptr_t *res, uintptr_t nod)
{
	uintptr_t chld;

	arr_get(&chld, res);
	nod_init(nod, type_sub, chld(chld, 0x0));
	arr_put(res, &nod);

	return 0;
}

int
parse_rep(uintptr_t *res, uintptr_t nod, struct token const *stk)
{
	uintptr_t chld;

	if (!arr_len(res)) return PAT_ERR_BADREP;
	arr_get(&chld, res);

	nod_init(nod, stk->op, chld(chld, 0x0));
	arr_put(res, &nod);

	return parse_next(res, 0x0, ++stk);
}
