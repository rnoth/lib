#include <errno.h>
#include <arr.h>
#include <pat.ih>

#define prec(tok) (tab_prec[((struct token*){0}=(tok))->id])

#define token(...) tok(__VA_ARGS__, 0, 0)
#define tok(i, c, ...) ((struct token[]){{ .id = i, .ch = c }})

#define stk_peek(t) ((struct token *)arr_peek(t))

enum state {
	st_init,
	st_str,
};

struct scanner {
	uint8_t const *src;
	struct token  *stk;
	struct token  *que;
	enum state     st;
};

static int scan_next(   struct scanner *sc);
static int scan_string( struct scanner *sc);
static int scan_step(   struct scanner *sc);

static int shunt_alt(   struct scanner *sc);
static int shunt_char(  struct scanner *sc);
static int shunt_close( struct scanner *sc);
static int shunt_dot(   struct scanner *sc);
static int shunt_eol(   struct scanner *sc);
static int shunt_esc(   struct scanner *sc);
static int shunt_open(  struct scanner *sc);
static int shunt_mon(   struct scanner *sc);

static void tok_pop_greater(struct token *, struct token *, int8_t);
static void tok_pop_until(struct token *, struct token *, enum type);

static int8_t const tab_prec[] = {
	[type_sub] = 0,
	[type_nop] = 0,
	[type_alt] = 1,
	[type_cat] = 2,
	[255] = 0,
};

static uint8_t const tab_oper[] = {
	['?'] = type_opt,
	['*'] = type_kln,
	['+'] = type_rep,
	['('] = type_sub,
	['.'] = class_dot,
};

static int (* const tab_shunt[])() = {
	['\0'] = shunt_eol,
	['\\'] = shunt_esc,
	['.']  = shunt_dot,
	['*']  = shunt_mon,
	['?']  = shunt_mon,
	['+']  = shunt_mon,
	['|']  = shunt_alt,
	['(']  = shunt_open,
	[')']  = shunt_close,
	[255]  = 0,
};

int
pat_scan(struct token **res, void const *src)
{
	size_t const len = strlen(src) + 1;
	struct scanner sc[1] = {{ .src = src, }};
	int err;

	sc->que = arr_alloc(sizeof *sc->que, len * 2);
	if (!sc->que) goto nomem;

	sc->stk = arr_alloc(sizeof *sc->stk, len);
	if (!sc->stk) goto nomem;

	err = scan_step(sc);
	if (err) goto finally;

finally:
	arr_free(sc->stk);

	if (err) arr_free(sc->que);
	else *res = sc->que;

	return err;

nomem:
	err = ENOMEM;
	goto finally;
}

int
scan_string(struct scanner *sc)
{
	if (sc->st == st_str) arr_put(sc->stk, token(type_cat));
	else sc->st = st_str;

	return scan_next(sc);
}

int
scan_next(struct scanner *sc)
{
	++sc->src;
	return scan_step(sc);
}

int
scan_step(struct scanner *sc)
{
	int (*sh)();

	sh = tab_shunt[*sc->src];
	if (sh) return sh(sc);
	else return shunt_char(sc);
}

int
shunt_alt(struct scanner *sc)
{
	tok_pop_greater(sc->que, sc->stk, tab_prec[type_alt]);
	arr_put(sc->stk, token(type_alt));

	sc->st = st_init;
	return scan_next(sc);
}

int
shunt_char(struct scanner *sc)
{
	arr_put(sc->que, token(type_lit, *sc->src));
	return scan_string(sc);
}

int
shunt_close(struct scanner *sc)
{
	struct token tok[1];

	tok_pop_until(sc->que, sc->stk, type_nop);

	if (!arr_len(sc->stk)) return PAT_ERR_BADPAREN;

	arr_get(tok, sc->stk);
	if (tok->id != type_nop) return PAT_ERR_BADPAREN;

	arr_put(sc->que, token(type_sub));
	sc->st = tok->ch;
	return scan_string(sc);
}

int
shunt_dot(struct scanner *sc)
{
	arr_put(sc->que, token(type_cls, tab_oper[*sc->src]));
	return scan_string(sc);
}

int
shunt_eol(struct scanner *sc)
{
	if (!arr_len(sc->stk)) return 0;
	arr_pop(sc->que, sc->stk);
	return shunt_eol(sc);
}

int
shunt_esc(struct scanner *sc)
{
	++sc->src;
	return shunt_char(sc);
}

int
shunt_open(struct scanner *sc)
{
	arr_put(sc->stk, token(type_nop, sc->st));

	sc->st = st_init;
	return scan_next(sc);
}

int
shunt_mon(struct scanner *sc)
{
	uint8_t ty = tab_oper[*sc->src];
	arr_put(sc->que, token(ty, *sc->src));
	return scan_next( sc);
}

void
tok_pop_greater(struct token *res, struct token *stk, int8_t pr)
{
	if (arr_len(stk) == 0) return;
	if (prec(arr_peek(stk)) < pr) return;

	arr_pop(res, stk);
	tok_pop_greater(res, stk, pr);
}

void
tok_pop_until(struct token *res, struct token *stk, enum type ty)
{
	if (arr_len(stk) == 0) return;
	if (stk_peek(stk)->id == ty) return;

	arr_pop(res, stk);
	tok_pop_until(res, stk, ty);
}
