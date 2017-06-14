#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <util.h>
#include <vec.h>
#include <pat.ih>
#include <pat.h>

#define instr(...) ins(__VA_ARGS__, 0,)
#define ins(OP, ARG, ...) (struct ins){ .op = tab_op[OP], .arg = ARG }

enum {
	from_above,
	from_left,
	from_right,
};

static void compile(struct ins *, struct token *, int);
static void comp_cat(struct ins *, struct token *, int);
static void comp_una(struct ins *, struct token *, int);
static void comp_mon(struct ins *, struct token *, int);
static void comp_nul(struct ins *, struct token *, int);

static int (* tab_op[])(struct context *, struct thread *, char const) = {
	[op_char] = do_char,
	[op_clss] = do_clss,
	[op_jump] = do_jump,
	[op_fork] = do_fork,
	[op_save] = do_save,
	[op_mark] = do_mark,
	[op_halt] = do_halt,
};

static void (* tab_comp[])(struct ins *, struct token *, int) = {
	[nop]     = comp_cat,
	[op_char] = comp_una,
	[op_clss] = comp_una,
	[op_jump] = comp_mon,
	[op_fork] = comp_mon,
	[op_save] = comp_nul,
	[op_mark] = comp_nul,
	[op_halt] = comp_nul,

};

struct token *
right(struct token *t)
{
	return t - 1;
}

struct token *
left(struct token *t)
{
	return right(t) - right(t)->len;
}

void
compile(struct ins *dst, struct token *tok, int n)
{
	tab_comp[tok->op](dst, tok, n);
}

void
walk_left(struct ins *dst, struct token *tok)
{
	left(tok)->up = tok;
	compile(dst, left(tok), from_above);
}

void
walk_right(struct ins *dst, struct token *tok)
{
	right(tok)->up = tok;
	compile(dst, right(tok), from_above);
}

void
walk_back(struct ins *dst, struct token *tok)
{
	if (tok->up == 0x0) return;
	else if (tok == left( tok->up)) compile(dst, tok->up, from_left);
	else if (tok == right(tok->up)) compile(dst, tok->up, from_right);
}

void
comp_cat(struct ins *dst, struct token *tok, int n)
{
	switch (n) {
	case from_above:
		walk_left(dst, tok);
		return;
	case from_left:
		walk_right(dst, tok);
		return;
	case from_right:
		walk_back(dst, tok);
		return;
	}
}

void
comp_una(struct ins *dst, struct token *tok, int n)
{
	*dst++ = instr(tok->op, {.b = tok->ch});
	walk_back(dst, tok);
}

void
comp_nul(struct ins *dst, struct token *tok, int n)
{
	*dst++ = instr(tok->op);
	walk_back(dst, tok);
}

void
comp_mon(struct ins *dst, struct token *tok, int n)
{
	switch (n) {
	case from_above:
		if (!tok->wh) *dst++ = instr(tok->op, tok->len - 1);
		walk_right(dst, tok);
		return;
	case from_right:
		if ( tok->wh) *dst++ = instr(tok->op, -tok->len + 1);
		walk_back(dst, tok);
		return;
	}
}

int
pat_marshal(struct pattern *pat, struct token *tok)
{
	pat->prog = calloc(tok->len, sizeof *pat->prog);
	if (!pat->prog) return ENOMEM;

	compile(pat->prog, tok, 0x0);

	return 0;
}
