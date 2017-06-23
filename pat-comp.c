#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <pat.ih>
#include <pat.h>

#define instr(...) ins(__VA_ARGS__, 0,)
#define ins(OP, ARG, ...) (struct ins){ .op = OP, .arg = ARG }

static struct token *chld_next(struct token *, struct token *);

static struct token *comp_alt(struct ins **, struct token *, struct token *);
static struct token *comp_lit(struct ins **, struct token *, struct token *);
static struct token *comp_cls(struct ins **, struct token *, struct token *);
static struct token *comp_reg(struct ins **, struct token *, struct token *);
static struct token *comp_kln(struct ins **, struct token *, struct token *);
static struct token *comp_nop(struct ins **, struct token *, struct token *);
static struct token *comp_opt(struct ins **, struct token *, struct token *);
static struct token *comp_rep(struct ins **, struct token *, struct token *);
static struct token *comp_sub(struct ins **, struct token *, struct token *);

static void marshal(struct ins *, struct token *tok);

static struct token *(* const tab_comp[])(struct ins **, struct token *, struct token *) = {
	[type_lit] = comp_lit,
	[type_cls] = comp_cls,
	[type_opt] = comp_opt,
	[type_rep] = comp_rep,
	[type_kln] = comp_kln,
	[type_alt] = comp_alt,
	[type_sub] = comp_sub,
	[type_reg] = comp_reg,
	[type_nop] = comp_nop,
};

static size_t tab_len[] = {
	[type_lit] = 1,
	[type_cls] = 1,
	[type_opt] = 1,
	[type_rep] = 1,
	[type_kln] = 2,
	[type_alt] = 2,
	[type_sub] = 2,
	[type_reg] = 6,
};

struct token *
chld_next(struct token *tok, struct token *ctx)
{
	size_t off;

	if (!ctx || tok < ctx) return tok - 1;
	if (tok == ctx) return tok->up;

	off = tok - ctx + ctx->siz;

	if (off >= tok->siz) return tok;

	return tok - off;
}

struct token *
comp_alt(struct ins **dst, struct token *tok, struct token *ctx)
{
	size_t off;

	if (tok == ctx) {
		*dst[0]-- = instr(do_fork, tok->siz + 1);
		return tok->up;
	}
	if (tok < ctx) {
		off = tok->up->len - tok->len - type_len(tok->up->id);
		*dst[0]-- = instr(do_jump, off + 1);
		return tok - 1;
	}

	return chld_next(tok, ctx);
}

struct token *
comp_opt(struct ins **dst, struct token *tok, struct token *ctx)
{
	if (tok == ctx) *dst[0]-- = instr(do_fork, tok->len);
	return chld_next(tok, ctx);
}

struct token *
comp_rep(struct ins **dst, struct token *tok, struct token *ctx)
{
	if (tok < ctx) *dst[0]-- = instr(do_fork, -tok->len + 1);
	else if (tok > ctx) return tok->up;
	return chld_next(tok, ctx);
}

struct token *
comp_sub(struct ins **dst, struct token *tok, struct token *ctx)
{
	if (tok == ctx) *dst[0]-- = instr(do_mark);
	if (tok < ctx) *dst[0]-- = instr(do_save);

	return chld_next(tok, ctx);
}

struct token *
comp_kln(struct ins **dst, struct token *tok, struct token *ctx)
{
	if (tok < ctx) *dst[0]-- = instr(do_fork, -tok->siz + 1);
	if (tok == ctx) *dst[0]-- = instr(do_fork, tok->siz);

	return chld_next(tok, ctx);
}

struct token *
comp_reg(struct ins **dst, struct token *tok, struct token *ctx)
{
	if (!ctx) {
		*dst[0]-- = instr(do_halt);
		*dst[0]-- = instr(do_save);
		return tok - 1;
	}
	if (tok == ctx) {
		*dst[0]-- = instr(do_mark);
		*dst[0]-- = instr(do_fork, -1);
		*dst[0]-- = instr(do_clss, 0);
		*dst[0]-- = instr(do_jump, 2);
		return 0x0;
	}

	return chld_next(tok, ctx);
}

struct token *
comp_lit(struct ins **dst, struct token *tok, struct token *ctx)
{
	*dst[0]-- = instr(do_char, {.b = tok->ch});
	return chld_next(ctx, tok);
}

struct token *
comp_cls(struct ins **dst, struct token *tok, struct token *ctx)
{
	*dst[0]-- = instr(do_clss, {.b = tok->ch});
	return chld_next(ctx, tok);
}

struct token *
comp_nop(struct ins **dst, struct token *tok, struct token *ctx)
{
	return chld_next(ctx, tok);
}

size_t
type_len(enum type ty)
{
	return tab_len[ty] ? tab_len[ty] : *(volatile size_t*)0x0;
}

void
marshal(struct ins *dst, struct token *tok)
{
	struct token *tmp = 0x0;
	struct token *ctx = 0x0;

	dst += tok->len - 1;

	while (tok) {
		tmp = tab_comp[tok->id](&dst, tok, ctx);

		if (!tmp) break;

		if (tmp->siz < tok->siz) {
			tmp->up = tok;
			ctx = tok;
		} else if (tmp->siz < ctx->siz) {
			tmp->up = ctx;
		} else if (ctx->siz < tmp->siz) {
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
