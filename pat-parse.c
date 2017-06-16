#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pat.ih>

#define token(...) tok(__VA_ARGS__, 0,)
#define tok(t, c, ...) ((struct token[]){{.op = t, .ch = c}})

enum state {
	st_init,
	st_esc,
	st_len,
};

struct parser {
	uint8_t const *src;
	struct token  *res;
	struct token  *tmp;
	enum state     st;
	size_t         len;
	size_t         siz;
};


static enum type oper(uint8_t const *);

static void pop_nop(struct parser *);

static void push_alt(struct parser *);
static void push_fini(struct parser *);
static void push_mon(struct parser *, struct token *);
static void push_nop(struct parser *);
static void push_rep(struct parser *, enum type);
static void push_res(struct parser *, struct token *);
static void push_rit(struct parser *);
static void push_var(struct parser *, struct token *);

static int shift(struct parser *);

static int shunt_alt(struct parser *);
static int shunt_dot(struct parser *);
static int shunt_esc(struct parser *);
static int shunt_eol(struct parser *);
static int shunt_lef(struct parser *);
static int shunt_lit(struct parser *);
static int shunt_rep(struct parser *);
static int shunt_rit(struct parser *);

static int parse(struct token **, struct parser *);

static int (* const tab_shunt[][st_len])() = {
	[0]    = { shunt_eol, shunt_eol, },
	['\\'] = { shunt_esc, },
	['?']  = { shunt_rep, },
	['+']  = { shunt_rep, },
	['*']  = { shunt_rep, },
	['|']  = { shunt_alt, },
	['(']  = { shunt_lef, },
	[')']  = { shunt_rit, },
	['.']  = { shunt_dot, },
	[255]  = {0},
};

enum type
oper(uint8_t const *src)
{
	static enum type const tab[] = {
		['*'] = type_kln,
		['+'] = type_rep,
		['?'] = type_opt,
	};

	return tab[*src];
}

void
pop_all(struct parser *pa)
{
	while (pa->tmp->op) *++pa->res = *pa->tmp--;
}

void
pop_nop(struct parser *pa)
{
	size_t off = pa->siz;
	struct token *tok = pa->res -off;
	pa->len += tok->len;
	pa->siz += tok->siz + 1;
	tok->len = 0;
	tok->siz = 1;
}

void
push_alt(struct parser *pa)
{
	*++pa->res = (struct token){
		.op  = type_alt,
		.len = pa->len += type_len(type_alt),
		.siz = pa->siz += 1,
	};
}

void
push_fini(struct parser *pa)
{
	push_var(pa, token(type_reg));
}

void
push_rit(struct parser *pa)
{
	push_var(pa, token(type_sub));
}

void
push_mon(struct parser *pa, struct token *tok)
{
	struct token *chld = pa->res;
	size_t len = type_len(tok->op);

	pa->siz += 1;
	pa->len += len;

	tok->siz = chld->siz + 1;
	tok->len = chld->len + len;

	*++pa->res = *tok;
}

void
push_nop(struct parser *pa)
{
	*++pa->res = (struct token){
		.op  = type_nop, 
		.len = pa->len,
		.siz = pa->siz,
	};

	pa->len = 0;
	pa->siz = 0;
}

void
push_res(struct parser *pa, struct token *tok)
{
	pa->siz += tok->siz = 1;
	pa->len += tok->len = type_len(tok->op);
	*++pa->res = *tok;
}

void
push_rep(struct parser *pa, enum type ty)
{
	push_mon(pa, token(ty));
}

void
push_var(struct parser *pa, struct token *tok)
{
	tok->siz = pa->siz += 1,
	tok->len = pa->len += type_len(tok->op),
	*++pa->res = *tok;
}

int
shift(struct parser *pa)
{
	int (*shunt)();
	int err;

	shunt = tab_shunt[*pa->src][pa->st];

	if (shunt) err = shunt(pa);
	else err = shunt_lit(pa);

	++pa->src;

	return err;
}

int
shunt_alt(struct parser *pa)
{
	push_alt(pa);
	return 0;
}

int
shunt_dot(struct parser *pa)
{
	push_res(pa, token(type_cls, class_dot));
	return 0;
}

int
shunt_esc(struct parser *pa)
{
	pa->st = st_esc;
	return 0;
}

int
shunt_eol(struct parser *pa)
{
	size_t off = pa->siz;
	if (pa->res[-off].op != type_nil) { // XXX
		return  PAT_ERR_BADPAREN;
	}

	push_fini(pa);
	return -1;
}

int
shunt_lef(struct parser *pa)
{
	push_nop(pa);
	return 0;
}

int
shunt_lit(struct parser *pa)
{
	push_res(pa, token(type_lit, *pa->src));
	return 0;
}

int
shunt_rep(struct parser *pa)
{
	push_rep(pa, oper(pa->src));
	return 0;
}

int
shunt_rit(struct parser *pa)
{
	size_t off = pa->siz;

	if (pa->res[-off].op != type_nop) { // XXX
		return  PAT_ERR_BADPAREN;
	}

	push_rit(pa);
	pop_nop(pa);

	return 0;
}

int
parse(struct token **dst, struct parser *pa)
{
	int err = 0;

	while (!err) err = shift(pa);
	if (err != -1) return err;

	*dst = pa->res;
	
	return 0;
}

int
parser_init(struct parser *pa, void const *src)
{
	size_t len = strlen(src);

	pa->res = calloc(len * 2 + 6, sizeof *pa->res);
	if (!pa->res) goto nomem;

	pa->src = src;

	return 0;

nomem:
	free(pa->res);
	free(pa->tmp);

	return ENOMEM;
}

void
tok_free(struct token *tok)
{
	if (!tok) return;
	while (tok->op) --tok;
	free(tok);
}

int
pat_parse(struct token **dst, char const *src)
{
	struct parser pa[1] = {0};
	int err;

	err = parser_init(pa, src);
	if (err) goto finally;

	err = parse(dst, pa);
	if (err) goto finally;

finally:
	if (err) tok_free(pa->res);
	tok_free(pa->tmp);

	return err;
}
