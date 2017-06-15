#include <errno.h>
#include <arr.h>
#include <pat.ih>

#define prec(tok) (tab_prec[((struct token*){0}=(tok))->op])

#define tok(...) _tok(__VA_ARGS__, 0,)
#define _tok(o, n, w, ...) ((struct token){ .op = o, .len = n, .wh = w})

#define lit(...) _lit(__VA_ARGS__, 0,)
#define _lit(o, c, ...) ((struct token){.op = o, .len = 1, .ch = c})

#define _helper(arr) arr, sizeof arr / sizeof *arr
#define add_tok(sc, ...) add_tokv(sc, _helper(((struct token[]){__VA_ARGS__})))
#define add_tmp(sc, ...) add_tmpv(sc, _helper(((struct token[]){__VA_ARGS__})))

enum state {
	st_init,
	st_str,
};

struct scanner {
	uint8_t const *src;
	struct token  *res;
	struct token  *tmp;
	size_t         len;
	enum   state   st;
};

static int scan_step(   struct scanner *sc);

static int shunt_alt(   struct scanner *sc);
static int shunt_char(  struct scanner *sc);
static int shunt_close( struct scanner *sc);
static int shunt_dot(   struct scanner *sc);
static int shunt_eol(   struct scanner *sc);
static int shunt_esc(   struct scanner *sc);
static int shunt_open(  struct scanner *sc);
static int shunt_mon(   struct scanner *sc);

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
	if (t->op == op_null) return t;
	--t;
	return unwind(t);
}

void
tok_free(struct token *t)
{
	free(unwind(t));
}

void
add_tokv(struct scanner *sc, struct token *tokv, size_t len)
{
	memcpy(sc->res + 1, tokv, len * sizeof *sc->res);
	sc->res += len;

	sc->len += len;
}

void
add_tmpv(struct scanner *sc, struct token *tokv, size_t len)
{
	memcpy(sc->tmp + 1, tokv, len * sizeof *sc->tmp);
	sc->tmp += len;
}

void
add_cat(struct scanner *sc)
{
	size_t off = sc->res[0].len;
	size_t len = off + sc->res[-off].len;
	*++sc->res = tok(nop, len);
}

void
add_lit(struct scanner *sc)
{
	add_tok(sc, lit(op_char, *sc->src));
}

void
add_init(struct scanner *sc)
{
	add_tok(sc, lit(op_clss, class_any),
	            tok(op_fork, 2, after),
	            tok(op_jump, 3, before));
}

void
add_fini(struct scanner *sc)
{
	add_tok(sc, tok(op_halt, sc->len + 1, after));
}

void
add_open(struct scanner *sc)
{
	add_tmp(sc, tok(nop, sc->len));
	sc->len = 0;
}

void
add_close(struct scanner *sc)
{
	struct token *nop = sc->tmp;
	add_tok(sc, tok(op_mark, sc->len + 1, before),
	            tok(op_save, sc->len + 2, after));
	sc->len += nop->len;
	--sc->tmp;
}

int
scanner_init(struct scanner *sc, void const *src)
{
	size_t len = strlen(src);

	sc->res = calloc(len * 2 + 10, sizeof *sc->res);
	if (!sc->res) goto nomem;

	sc->tmp = calloc(len, sizeof *sc->tmp);
	if (!sc->tmp) goto nomem;

	sc->src = src;

	return 0;

nomem:
	free(sc->tmp);
	free(sc->res);
	return ENOMEM;
}

int
scan(struct scanner *sc)
{
	int err = 0;

	add_init(sc);
	add_open(sc);

	while (!err) err = scan_step(sc);
	if (err != -1) return err;
	
	add_close(sc);
	add_fini(sc);

	return 0;
}

int
pat_scan(struct token **res, char const *src)
{
	struct scanner sc[1] = {{0}};
	int err;

	err = scanner_init(sc, src);
	if (err) goto finally;

	err = scan(sc);
	if (err) goto finally;

finally:
	tok_free(sc->tmp);

	if (err) tok_free(sc->tmp);
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
scan_step(struct scanner *sc)
{
	int (*shunt)();
	int err;

	shunt = tab_shunt[*sc->src];

	if (shunt) err = shunt(sc);
	else err = shunt_char(sc);

	++sc->src;
	return err;
}

int
shunt_alt(struct scanner *sc)
{
	return -1;
}

int
shunt_char(struct scanner *sc)
{
	add_lit(sc);
	return 0;
}

int
shunt_close(struct scanner *sc)
{
	return -1;
}

int
shunt_dot(struct scanner *sc)
{
	return -1;
}

int
shunt_eol(struct scanner *sc)
{
	return -1;
}

int
shunt_esc(struct scanner *sc)
{
	++sc->src; // XXX
	return shunt_char(sc);
}

int
shunt_open(struct scanner *sc)
{
	add_open(sc);
	return 0;
}

int
shunt_mon(struct scanner *sc)
{
	return -1;
}
