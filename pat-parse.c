#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pat.ih>

#define token(t, c) ((struct token[]){{.op = t, .ch = c}})

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

static void push_res(struct parser *, struct token *);

static int shift(struct parser *);

static int shunt_esc(struct parser *);
static int shunt_eol(struct parser *);
static int shunt_lit(struct parser *);

static int parse(struct token **, struct parser *);

static int (* const tab_shunt[][st_len])() = {
	[0]    = { shunt_eol, shunt_eol, },
	['\\'] = { shunt_esc, },
	[255]  = {0},
};

void
pop_nop(struct parser *pa)
{
	struct token *tok = pa->tmp;
	if (tok->op != nop) return;
	else pa->tmp--;
	pa->len += tok->len;
	pa->siz += tok->siz;
}

void
push_res(struct parser *pa, struct token *src)
{
	pa->siz += src->siz = 1;
	pa->len += src->len = type_len(src->op);
	*++pa->res = *src;
}

void
push_nop(struct parser *pa, enum type ty)
{
	*++pa->tmp = (struct token){
		.op  = nop, 
		.ch  = ty,
		.len = pa->len,
		.siz = pa->siz,
	};

	pa->len = 0;
	pa->siz = 0;
}

void
push_kln(struct parser *pa)
{
	*++pa->res = (struct token){
		.op  = type_kln,
		.siz = pa->siz += 1,
		.len = pa->len += type_len(type_kln),
	};
}

void
push_reg(struct parser *pa)
{
	*++pa->res = (struct token){
		.op = type_reg,
		.siz = pa->siz += 1,
		.len = pa->len += type_len(type_reg),
	};
}

void
push_fini(struct parser *pa)
{
	push_reg(pa);
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
shunt_esc(struct parser *pa)
{
	pa->st = st_esc;
	return -2;
}

int
shunt_eol(struct parser *pa)
{
	if (pa->tmp->op) return PAT_ERR_BADPAREN;
	push_fini(pa);
	return -1;
}

int
shunt_lit(struct parser *pa)
{
	push_res(pa, token(type_lit, *pa->src));

	return 0;
}

int
parse(struct token **dst, struct parser *pa)
{
	int err = 0;

	while (!err) err = tab_shift[pa->st](pa);
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

	pa->tmp = calloc(len, sizeof *pa->tmp);
	if (!pa->tmp) goto nomem;

	pa->src = src;

	return 0;

nomem:
	free(pa->res);
	free(pa->tmp);

	return ENOMEM;
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
