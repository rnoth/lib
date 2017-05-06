#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "pat.h"
#include "util.h"
#include "vec.h"

#define HALT (pat_ins_halt)
#define CHAR (pat_ins_char)
#define FORK (pat_ins_fork)
#define JUMP (pat_ins_jump)
#define MARK (pat_ins_mark)
#define SAVE (pat_ins_save)
#define CLSS (pat_ins_clss)

enum pat_char_class {
	pat_cl_any   = 0x0,
	pat_cl_dot   = 0x1,
	pat_cl_alpha = 0x2,
	pat_cl_upper = 0x3,
	pat_cl_lower = 0x4,
	pat_cl_digit = 0x5,
	pat_cl_space = 0x6,
};

enum pat_sym {
	pat_sym_literal,
	pat_sym_dot,
	pat_sym_qmark,
	pat_sym_star,
	pat_sym_plus,
	pat_sym_pipe,
	pat_sym_carrot,
	pat_sym_dollar,
	pat_sym_lparen,
	pat_sym_rparen,
};

struct context {
	size_t		  pos;
	struct thread *thr;
	struct thread *mat;
};

struct thread {
	struct patmatch *mat;
	struct patins 	*ins;
	size_t		 pos;
};

struct token {
	size_t		 len;
	enum pat_sym	 type;
	wchar_t		 wc;
};

struct patins {
        int      (*op)(struct context *, struct thread *, wchar_t const);
	uint64_t  arg;
};

static inline struct patins *ip(struct thread *);

static int  ctx_init(struct context *);
static void ctx_fini(struct context *);

static int  thr_next(struct context *, size_t, wchar_t const);
static int  thr_finish(struct context *, size_t);
static int  thr_fork(struct thread *, struct thread *);
static int  thr_start(struct context *, wchar_t const);
static void thr_remove(struct context *, size_t);

static int pataddalt(struct pattern *, struct token *, struct patins **);
static int pataddbol(struct pattern *, struct token *, struct patins **);
static int pataddeol(struct pattern *, struct token *, struct patins **);
static int pataddlit(struct pattern *, struct token *, struct patins **);
static int pataddlpar(struct pattern *, struct token *, struct patins **);
static int pataddrpar(struct pattern *, struct token *, struct patins **);
static int pataddrep(struct pattern *, struct token *, struct patins **);

static int pat_ins_char(struct context *, struct thread *, wchar_t const);
static int pat_ins_clss(struct context *, struct thread *, wchar_t const);
static int pat_ins_fork(struct context *, struct thread *, wchar_t const);
static int pat_ins_halt(struct context *, struct thread *, wchar_t const);
static int pat_ins_jump(struct context *, struct thread *, wchar_t const);
static int pat_ins_mark(struct context *, struct thread *, wchar_t const);
static int pat_ins_save(struct context *, struct thread *, wchar_t const);

static int pataddinit(struct pattern *);
static int pataddfini(struct pattern *, struct patins **);

static size_t patlex(struct token *, char const *, size_t);
static int    pat_do_match(struct patmatch **, struct context *, char const *);

static int (* const patadd[])(struct pattern *, struct token *, struct patins **) = {
	[pat_sym_literal] = pataddlit,
	[pat_sym_qmark]	  = pataddrep,
	[pat_sym_star]	  = pataddrep,
	[pat_sym_plus]	  = pataddrep,
	[pat_sym_pipe]    = pataddalt,
	[pat_sym_carrot]  = pataddbol,
	[pat_sym_dollar]  = pataddeol,
	[pat_sym_lparen]  = pataddlpar,
	[pat_sym_rparen]  = pataddrpar,
};

int
ctx_init(struct context *ctx)
{
	int err = 0;

	err = vec_ctor(ctx->thr);
	if (err) goto nomem;

	err = vec_ctor(ctx->mat);
	if (err) goto nomem;

	return 0;
nomem:
	vec_free(ctx->thr);
	vec_free(ctx->mat);
	return err;

}

void
ctx_fini(struct context *ctx)
{
	struct thread *tmp;

	vec_foreach(tmp, ctx->thr) vec_free(tmp->mat);
	vec_free(ctx->thr);
	vec_free(ctx->mat);
}

int
thr_finish(struct context *ctx, size_t ind)
{
	int err = 0;

	err = vec_append(&ctx->mat, ctx->thr + ind);
	if (err) return err;
	vec_delete(&ctx->thr, ind);

	return 0;
}

int
thr_fork(struct thread *dst, struct thread *src)
{
	dst->ins = src->ins;
	dst->pos = src->pos;
	dst->mat = vec_clone(src->mat);

	return dst->mat ? 0 : ENOMEM;
}

struct patins *
ip(struct thread *th)
{
	return th->ins + th->pos;
}

int
thr_next(struct context *ctx, size_t ind, wchar_t wc)
{
	if (ind >= vec_len(ctx->thr)) return 0;
	else return ip(ctx->thr + ind)->op(ctx, ctx->thr + ind, wc);
}

int
thr_start(struct context *ctx, wchar_t wc)
{
	return ip(ctx->thr)->op(ctx, ctx->thr, wc);
}

void
thr_remove(struct context *ctx, size_t ind)
{
	vec_free(ctx->thr[ind].mat);
	vec_delete(&ctx->thr, ind);
}

int // XXX
pataddalt(struct pattern *pat, struct token *tok, struct patins **bufp)
{
	int err = 0;
	size_t offset = 0;
	struct patins ins = {0};

	if (addz_overflows(vec_len(pat->prog), len(*bufp)))
		return EOVERFLOW;
	offset = vec_len(pat->prog) + len(*bufp);
	if (addz_overflows(offset, 2))
		return EOVERFLOW;
	offset += 2;

	ins.op = FORK;
	ins.arg = offset;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	err = vec_join(&pat->prog, *bufp);
	if (err) return err;

	ins.op = JUMP;
	ins.arg = -1;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	vec_truncate(bufp, 0);

	return 0;
}

int
pataddbol(struct pattern *pat, struct token *tok, struct patins **bufp)
{
	vec_shift(&pat->prog, 3);
	return 0;
}

int
pataddeol(struct pattern *pat, struct token *tok, struct patins **bufp)
{
	int err;
	struct patins ins = { .op = CHAR, .arg = 0 };
	err = vec_join(&pat->prog, *bufp);
	if (err) return err;
	vec_truncate(bufp, 0);
	return vec_append(&pat->prog, &ins);
}

int
pataddinit(struct pattern *pat)
{
	struct patins insv[] = {
		[0] = { .op = JUMP, .arg = 2 },
		[1] = { .op = CLSS, .arg = 0 },
		[2] = { .op = FORK, .arg = 1 },
		[3] = { .op = MARK, .arg = 0 },
	};

	return vec_concat(&pat->prog, insv, sizeof insv / sizeof *insv);
}

int
pataddfini(struct pattern *pat, struct patins **bufp)
{
	int err = 0;
	struct patins insv[] = {
		[0] = { .op = SAVE, .arg = 0 },
		[1] = { .op = HALT, .arg = 0 },
	};

	err = vec_join(&pat->prog, *bufp);
	if (err) return err;

	vec_truncate(bufp, 0);

	return vec_concat(&pat->prog, &insv, sizeof insv/ sizeof *insv);
}

int
pataddlit(struct pattern *pat, struct token *tok, struct patins **bufp)
{
	struct patins ins = { .op = CHAR, .arg = tok->wc };
	return vec_append(bufp, &ins);
}

int
pataddlpar(struct pattern *pat, struct token *tok, struct patins **bufp)
{
	int err = 0;
	struct patins ins = { .op = JUMP, .arg = -2 };

	err = vec_join(&pat->prog, *bufp);
	if (err) return err;

	vec_truncate(bufp, 0);

	return vec_append(&pat->prog, &ins);
}

int
pataddrep(struct pattern *pat, struct token *tok, struct patins **bufp)
{
	int err = 0;
	struct patins ins = {0};
	size_t again = vec_len(pat->prog);
	size_t offset = tok->type != pat_sym_qmark ? 3 : 2;

	if (!vec_len(*bufp)) return 0;

	if (addz_overflows(vec_len(pat->prog), vec_len(*bufp) + offset))
		return EOVERFLOW;

	err = vec_concat(&pat->prog, *bufp, vec_len(*bufp) - 1);
	if (err) return err;

	if (tok->type != pat_sym_plus) {
		ins.op = FORK;
		ins.arg = vec_len(pat->prog) + offset;

		err = vec_append(&pat->prog, &ins);
		if (err) return err;

		++again;
	}

	ins = (*bufp)[vec_len(*bufp) - 1];

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	if (tok->type != pat_sym_qmark) {
		ins.op = FORK;
		ins.arg = again;

		err = vec_append(&pat->prog, &ins);
		if (err) return err;
	}

	vec_truncate(bufp, 0);

	return 0;
}

int
pataddrpar(struct pattern *pat, struct token *tok, struct patins **bufp)
{
	int err = 0;
	size_t i = 0;
	uint64_t arg = 0;
	struct patins ins = {0};
	struct patins *p = 0x0;

	err = vec_join(&pat->prog, *bufp);
	if (err) return err;

	vec_truncate(bufp, 0);

	for (i = vec_len(pat->prog); p = pat->prog + i, i --> 0;) {
		if (p->op == JUMP && p->arg > -3UL) {
			arg = p->arg;
			p->arg = vec_len(pat->prog);
			if (arg == -2UL) break;
			else ++p->arg;
		}
	}

	ins.op = JUMP;
	ins.arg = vec_len(pat->prog) + 1;
	if (ins.arg < vec_len(pat->prog)) return EOVERFLOW;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	ins.op = JUMP;
	ins.arg = i + 1;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	return 0;
}

int
pat_ins_char(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;

	if ((wchar_t)ip(th)->arg != wc) thr_remove(ctx, th - ctx->thr), --ind;
	else ++th->pos;

	return thr_next(ctx, ind + 1, wc);
}

int
pat_ins_clss(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;
	bool res;

	switch (ip(th)->arg) {
	case pat_cl_any: res = true; break;
	case pat_cl_dot: res = wc != L'\n' && wc != L'\0'; break;
	case pat_cl_alpha: res = iswalpha(wc); break;
	case pat_cl_upper: res = iswupper(wc); break;
	case pat_cl_lower: res = iswlower(wc); break;
	case pat_cl_space: res = iswspace(wc); break;
	case pat_cl_digit: res = iswdigit(wc); break;
	default:
		assert(!"invalid argument to character class instruction");
	}

	if (!res) thr_remove(ctx, ind), --ind;
	else ++th->pos;

	return thr_next(ctx, ind + 1, wc);
}

int
pat_ins_fork(struct context *ctx, struct thread *th, wchar_t const wc)
{
	int err = 0;
	struct thread new = {0};

	err = thr_fork(&new, th);
	if (err) goto fail;

	new.pos = ip(th)->arg;

	err = vec_insert(&ctx->thr, &new, th - ctx->thr + 1);
	if (err) goto fail;

	++th->pos;
	return ip(th)->op(ctx, th, wc);

fail:
	vec_free(th->mat);
	free(th);
	return err;
}

int
pat_ins_halt(struct context *ctx, struct thread *th, wchar_t const wc)
{
	int err = 0;
	size_t ind = th - ctx->thr;

	err = thr_finish(ctx, ind);
	if (err) return err;

	return thr_next(ctx, ind + 1, wc);
}

int
pat_ins_jump(struct context *ctx, struct thread *th, wchar_t const wc)
{
	th->pos = ip(th)->arg;
	return ip(th)->op(ctx, th, wc);
}

int
pat_ins_mark(struct context *ctx, struct thread *th, wchar_t const wc)
{
	int err = 0;
	struct patmatch mat = {0};

	mat.off = ctx->pos;
	mat.ext = -1;

	err = vec_append(&th->mat, &mat);
	if (err) return err;

	++th->pos;
	return ip(th)->op(ctx, th, wc);
}

int
pat_ins_save(struct context *ctx, struct thread *th, wchar_t const wc)
{
	struct patins *ins = th->ins + th->pos;

	th->mat[ins->arg].ext = ctx->pos - th->mat[ins->arg].off;
	++th->pos;

	return ins->op(ctx, th, wc);
}

size_t
patlex(struct token *tok, char const *src, size_t off)
{
	size_t ret = 0;
	int len = 0;

	ret = tok->len = 1;
	switch (src[off]) {
	case '.':
		tok->type = pat_sym_dot;
		break;
	case '?':
		tok->type = pat_sym_qmark;
		break;
	case '*':
		tok->type = pat_sym_star;
		break;
	case '+':
		tok->type = pat_sym_plus;
		break;
	case '(':
		tok->type = pat_sym_lparen;
		break;
	case ')':
		tok->type = pat_sym_rparen;
		break;
	case '|':
		tok->type = pat_sym_pipe;
		break;
	case '^':
		if (off) goto literal;
		tok->type = pat_sym_carrot;
		break;
	case '$':
		tok->type = pat_sym_dollar;
		break;
	case '\\':
		goto esc;
	default:
	literal:
		tok->type = pat_sym_literal;
		ret = tok->len = mbtowc(&tok->wc, src + off, strlen(src + off));
	}

	return ret;

esc:
	switch (src[1]) {
	default:
		tok->type = pat_sym_literal;
		len = mbtowc(&tok->wc, src, strlen(src + ret));
		if (len == -1) abort();
		ret = tok->len += len;
	}

	return ret;
}

int
pat_do_match(struct patmatch **res, struct context *ctx, char const *str)
{
	int err = 0;
	int len = 0;
	size_t max = 0;
	size_t srclen = strlen(str);
	struct thread *it;
	wchar_t wc = 0;

	while (str[ctx->pos]) {

		ctx->pos += len;

		len = mbtowc(&wc, str + ctx->pos, srclen - ctx->pos);
		if (len == -1) { err = EILSEQ; break; }

		err = thr_start(ctx, wc);
		if (err) break;
	}

	if (err > 0) goto finally;

	if (!len(ctx->mat)) {
		err = -1;
		goto finally;
	}
	
	vec_foreach (it, ctx->mat) {
		if (it->mat[0].ext > ctx->mat[max].mat[0].ext) {
			max = it - ctx->mat;
		}
	}

	err = vec_copy(res, ctx->mat[max].mat);

finally:
	return err;
}

int
pat_compile(struct pattern *dest, char const *src)
{
	int err = 0;
	size_t off = 0;
	struct token tok = {0};
	struct patins *buf = 0x0;

	vec_ctor(buf);
	if (!buf) return ENOMEM;

	vec_ctor(dest->prog);
	if (!dest->prog) goto finally;

	pataddinit(dest);

	while (src[off]) {
		off += patlex(&tok, src, off);
		err = patadd[tok.type](dest, &tok, &buf);
		if (err) goto finally;
	}

	pataddfini(dest, &buf);

finally:
	vec_free(buf);
	if (err) vec_free(dest->prog);
	return err;
}

int
pat_match(struct patmatch **matv, char const *str, struct pattern const *pat)
{
	int err = 0;
	struct context ctx = {0};
	struct thread th = { .ins = pat->prog, .pos = 0, .mat = 0x0 };

	if (matv) vec_check(matv);

	err = ctx_init(&ctx);
	if (err) goto finally;

	err = vec_ctor(th.mat);
	if (err) goto finally;

	err = vec_append(&ctx.thr, &th);
	if (err) goto finally;

	err = pat_do_match(matv, &ctx, str);
	if (err) goto finally;

finally:
	ctx_fini(&ctx);
	return err;
}

void
pat_free(struct pattern *pat)
{
	vec_free(pat->prog);
}
