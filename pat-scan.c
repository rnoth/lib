#include <errno.h>
#include <arr.h>
#include <pat.ih>

#define prec(tok) (tab_prec[((struct token*){0}=(tok))->id])

#define token(...) tok(__VA_ARGS__, 0, 0)
#define tok(i, n, c, ...) ((struct token){ .id = i, .len = n, .ch = c })

#define stk_peek(t) ((struct token *)arr_peek(t))

enum state {
	st_init,
	st_str,
};

struct scanner {
	uint8_t const *beg;
	uint8_t const *src;
	struct token  *res;
	struct token  *tmp;
	enum   state   st;
};

static enum type oper(uint8_t const *);

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

static void tok_pop_greater(struct token **, struct token **, enum type);
static void tok_pop_until(struct token **, struct token **, enum type);

static int8_t const tab_prec[] = {
	[type_sub] = 1,
	[type_nop] = 1,
	[type_alt] = 2,
	[type_cat] = 3,
	[type_rep] = 4,
	[type_opt] = 4,
	[type_kln] = 4,
	[type_lit] = 5,
	[type_cls] = 5,
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


struct token *
unwind(struct token *t)
{
	if (t->id == type_nil) return t;
	--t;
	return unwind(t);
}

void
add_cat(struct scanner *sc)
{
	size_t len = sc->res[0].len + sc->res[-1].len;
	*++sc->res = token(type_cat, len);
}

void
add_lit(struct scanner *sc)
{
	*++sc->res = token(type_lit, 1, *sc->src);
}

int
scanner_init(struct scanner *sc, void const *src)
{
	size_t len = strlen(src);

	memset(sc, 0, sizeof *sc);

	sc->res = calloc(len * 2, sizeof *sc->res);
	if (!sc->res) goto nomem;

	sc->tmp = calloc(len, sizeof *sc->tmp);
	if (!sc->tmp) goto nomem;

	sc->beg = src;
	sc->src = sc->beg + len;

	return 0;

nomem:
	free(sc->tmp);
	free(sc->res);
	return ENOMEM;
}

int
pat_scan(struct token **res, char const *src)
{
	struct scanner sc[1];
	int err;

	err = scanner_init(sc, src);
	if (err) goto finally;

	err = scan_step(sc);
	if (err) goto finally;

finally:
	free(unwind(sc->tmp));

	if (err) free(unwind(sc->res));
	else *res = sc->res;

	return err;
}

enum type
oper(uint8_t const *src)
{
	static uint8_t const tab[] = {
		['?'] = type_opt,
		['*'] = type_kln,
		['+'] = type_rep,
		['('] = type_sub,
		['.'] = class_dot,
	};

	return tab[*src];
}

int
scan_string(struct scanner *sc)
{
	if (sc->st == st_str) add_cat(sc);
	else sc->st = st_str;

	return scan_step(sc);
}

int
scan_init(struct scanner *sc)
{
	sc->st = st_init;
	return scan_step(sc);
}

int
scan_step(struct scanner *sc)
{
	int (*shunt)();

	if (sc->src == sc->beg) return shunt_eol(sc);
	else --sc->src;

	shunt = tab_shunt[*sc->src];
	if (shunt) return shunt(sc);
	else return shunt_char(sc);
}

int
shunt_alt(struct scanner *sc)
{
	tok_pop_greater(&sc->res, &sc->tmp, type_alt);
	*++sc->tmp = token(type_alt, 0);
	return -1; // scan_init(sc);
}

int
shunt_char(struct scanner *sc)
{
	add_lit(sc);
	return scan_string(sc);
}

int
shunt_close(struct scanner *sc)
{
	tok_pop_until(&sc->res, &sc->tmp, type_nop);
	if (sc->tmp->id == type_nil) return PAT_ERR_BADPAREN;

	sc->st = sc->tmp--->ch;

	*sc->res++ = token(type_sub, 0);
	return -1; //scan_string(sc);
}

int
shunt_dot(struct scanner *sc)
{
	*sc->res++ = token(type_cls, class_dot);
	return scan_string(sc);
}

int
shunt_eol(struct scanner *sc)
{
	tok_pop_greater(&sc->res, &sc->tmp, type_nil);
	return 0;
}

int
shunt_esc(struct scanner *sc)
{
	--sc->src;
	return shunt_char(sc);
}

int
shunt_open(struct scanner *sc)
{
	*++sc->tmp = token(type_nop, sc->st);
	return scan_init(sc);
}

int
shunt_mon(struct scanner *sc)
{
	*sc->res++ = token(oper(sc->src), 0);
	return -1; //scan_step(sc);
}

void
tok_pop_greater(struct token **stk, struct token **aux, enum type ty)
{
	if (aux[0]->id == type_nil) return;
	if (tab_prec[aux[0]->id] < tab_prec[ty]) return;

	*++stk[0] = *aux[0]--;
	tok_pop_greater(stk, aux, ty);
}

void
tok_pop_until(struct token **stk, struct token **aux, enum type ty)
{
	if (aux[0][-1].id == type_nil) return;
	if (aux[0][-1].id == ty) return;

	*++stk[0] = *aux[0]--;
	tok_pop_until(stk, aux, ty);
}
