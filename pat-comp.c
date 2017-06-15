#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <util.h>
#include <pat.ih>
#include <pat.h>

#define instr(...) ins(__VA_ARGS__, 0,)
#define ins(OP, ARG, ...) (struct ins){ .op = tab_op[OP], .arg = ARG }

static struct token *comp_nul(struct ins **, struct token *, struct token *);
static struct token *comp_mon(struct ins **, struct token *, struct token *);

static int (* const tab_op[])(struct context *, struct thread *, char const) = {
	[op_char] = do_char,
	[op_clss] = do_clss,
	[op_jump] = do_jump,
	[op_fork] = do_fork,
	[op_save] = do_save,
	[op_mark] = do_mark,
	[op_halt] = do_halt,
};

static struct token *(* const tab_comp[])(struct ins **, struct token *, struct token *) = {
	[op_char] = comp_nul,
	[op_clss] = comp_nul,
	[op_jump] = comp_mon,
	[op_fork] = comp_mon,
	[op_save] = comp_mon,
	[op_mark] = comp_mon,
	[op_halt] = comp_mon,

};

struct token *
chld_right(struct token *tok)
{
	if (tok->len == 1) return 0x0;
	else return tok - 1;
}

struct token *
chld_next(struct token *tok, struct token *ctx)
{
	size_t off;

	if (!ctx || tok < ctx) return chld_right(tok);
	if (tok == ctx) return tok->up;

	off = tok - ctx + ctx->len;

	if (off >= tok->len) return tok;

	return tok - off;
}

struct token *
comp_nul(struct ins **dst, struct token *tok, struct token *ctx)
{
	*dst[0]-- = instr(tok->op, {.b = tok->ch});
	return chld_next(ctx, tok);
}

struct token *
comp_mon(struct ins **dst, struct token *tok, struct token *ctx)
{
	if (ctx == tok) {
		if (tok->wh) return tok->up;
		*dst[0]-- = instr(tok->op, tok->len - 1);
		return tok->up;
	}

	if (!ctx || ctx > tok) {
		if (!tok->wh) return chld_right(tok);
		*dst[0]-- = instr(tok->op, -tok->len + 1);
		return chld_right(tok);
	}

	return chld_next(tok, ctx);
}

void
marshal(struct ins *dst, struct token *tok)
{
	struct token *tmp = 0x0;
	struct token *ctx = 0x0;

	dst += tok->len - 1;

	while (tok) {
		tmp = tab_comp[tok->op](&dst, tok, ctx);

		if (!tmp) break;

		if (tmp->len < tok->len) {
			tmp->up = tok;
			ctx = tok;
		} else if (tmp->len > ctx->len) {
			ctx = tok;
		}

		tok = tmp;
	}
}

int
pat_marshal(struct pattern *pat, struct token *tok)
{
	pat->prog = calloc(tok->len, sizeof *pat->prog);
	if (!pat->prog) return ENOMEM;

	marshal(pat->prog, tok);

	return 0;
}
