#include <errno.h>
#include <arr.h>
#include <pat.ih>

#define prec(tok) (tab_prec[(tok)->id])

#define token(...) tok(__VA_ARGS__, 0, 0)
#define tok(i, c, ...) ((struct token[]){{ .id = i, .ch = c }})

enum state {
	st_init,
	st_str,
};

static uint8_t oper(uint8_t const *);

static int shunt_next(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_alt(struct token *, struct token *, uint8_t const *);
static int shunt_char(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_close(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_eol(struct token *, struct token *);
static int shunt_esc(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_open(struct token *, struct token *, uint8_t const *, enum state);
static int shunt_mon(struct token *, struct token *, uint8_t const *);

static void tok_pop_greater(struct token *, struct token *, int8_t);

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
pat_scan(struct token **stk, void const *src)
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
