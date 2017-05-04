#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "pat.h"
#include "util.h"
#include "vec.h"

#define HALT (patdohalt)
#define CHAR (patdochar)
#define FORK (patdofork)
#define JUMP (patdojump)
#define MARK (patdomark)
#define SAVE (patdosave)
#define CLSS (patdoclss)

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

enum patret {
	MISMATCH = -1,
	MATCH    = -2,
	STEP     = -3,
};

struct patcontext {
	size_t		  pos;
	size_t		  nmat;
	struct patthread *thr;
};

struct patthread {
	struct patmatch *mat;
	struct patins 	*ins;
	size_t		 pos;
};

struct pattok {
	size_t		 len;
	enum pat_sym	 type;
	wchar_t		 wc;
};

struct patins {
        int      (*op)(struct patcontext *, struct patthread *, char const *);
	uint64_t  arg;
};

static inline struct patins *ip(struct patthread *);

static int pataddalt(struct pattern *, struct pattok *, struct patins **);
static int pataddbol(struct pattern *, struct pattok *, struct patins **);
static int pataddeol(struct pattern *, struct pattok *, struct patins **);
static int pataddlit(struct pattern *, struct pattok *, struct patins **);
static int pataddlpar(struct pattern *, struct pattok *, struct patins **);
static int pataddrpar(struct pattern *, struct pattok *, struct patins **);
static int pataddrep(struct pattern *, struct pattok *, struct patins **);

static int patdochar(struct patcontext *, struct patthread *, char const *);
static int patdoclss(struct patcontext *, struct patthread *, char const *);
static int patdofork(struct patcontext *, struct patthread *, char const *);
static int patdohalt(struct patcontext *, struct patthread *, char const *);
static int patdojump(struct patcontext *, struct patthread *, char const *);
static int patdomark(struct patcontext *, struct patthread *, char const *);
static int patdosave(struct patcontext *, struct patthread *, char const *);
//static int patdorefr(struct patcontext *, struct patthread *, char const *);

static int pataddinit(struct pattern *);
static int pataddfini(struct pattern *, struct patins **);

static size_t patlex(struct pattok *, char const *, size_t);
static int patmatch(struct patmatch **, struct patcontext *, char const *);
static int patstep(struct patthread **, struct patcontext *, char const *);
static struct patthread patfork(struct patthread *);

static int (* const patadd[])(struct pattern *, struct pattok *, struct patins **) = {
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

struct patins *
ip(struct patthread *th)
{
	return th->ins + th->pos;
}

int // XXX
pataddalt(struct pattern *pat, struct pattok *tok, struct patins **bufp)
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
pataddbol(struct pattern *pat, struct pattok *tok, struct patins **bufp)
{
	vec_shift(&pat->prog, 3);
	return 0;
}

int
pataddeol(struct pattern *pat, struct pattok *tok, struct patins **bufp)
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
pataddlit(struct pattern *pat, struct pattok *tok, struct patins **bufp)
{
	struct patins ins = { .op = CHAR, .arg = tok->wc };
	return vec_append(bufp, &ins);
}

int
pataddlpar(struct pattern *pat, struct pattok *tok, struct patins **bufp)
{
	int err = 0;
	struct patins ins = { .op = JUMP, .arg = -2 };

	err = vec_join(&pat->prog, *bufp);
	if (err) return err;

	vec_truncate(bufp, 0);

	return vec_append(&pat->prog, &ins);
}

int
pataddrep(struct pattern *pat, struct pattok *tok, struct patins **bufp)
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
pataddrpar(struct pattern *pat, struct pattok *tok, struct patins **bufp)
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
patdohalt(struct patcontext *ctx, struct patthread *th, char const *str)
{
	return MATCH;
}

int
patdochar(struct patcontext *ctx, struct patthread *th, char const *str)
{
	int len = 0;
	wchar_t wc = 0;

	len = mbtowc(&wc, str, strlen(str));
	if (len < 0) return errno;

	if ((wchar_t)ip(th)->arg != wc) return MISMATCH;

	++th->pos;
	return STEP;
}

int
patdojump(struct patcontext *ctx, struct patthread *th, char const *str)
{
	th->pos = ip(th)->arg;
	return ip(th)->op(ctx, th, str);
}

int
patdofork(struct patcontext *ctx, struct patthread *th, char const *str)
{
	int err = 0;
	struct patthread new = {0};

	new = patfork(th);
	if (!new.mat) return ENOMEM;

	new.pos = ip(th)->arg;

	err = vec_insert(&ctx->thr, &new, th - ctx->thr + 1);
	if (err) goto fail;

	++th->pos;
	return ip(th)->op(ctx, th, str);

fail:
	vec_free(th->mat);
	free(th);
	return err;
}

int
patdomark(struct patcontext *ctx, struct patthread *th, char const *str)
{
	int err = 0;
	struct patmatch mat = {0};

	mat.off = ctx->pos;
	mat.ext = -1;

	err = vec_append(&th->mat, &mat);
	if (err) return err;

	++th->pos;
	return ip(th)->op(ctx, th, str);
}

int
patdosave(struct patcontext *ctx, struct patthread *th, char const *str)
{
	struct patins *ins = th->ins + th->pos;
	th->mat[ins->arg].ext = ctx->pos - th->mat[ins->arg].off;
	++th->pos;
	return ins->op(ctx, th, str);
}

int
patdoclss(struct patcontext *ctx, struct patthread *th, char const *str)
{
	wchar_t wc = 0;
	int len = 0;

	len = mbtowc(&wc, str, strlen(str));
	if (len == -1) return errno;

	++th->pos;

	switch (th->ins[th->pos].arg) {
	case pat_cl_any:
		return STEP;
	case pat_cl_dot:
		return *str || *str != '\n' ? STEP : MISMATCH;
	case pat_cl_alpha:
		return iswalpha(wc) ? STEP : MISMATCH;
	case pat_cl_upper:
		return iswupper(wc) ? STEP : MISMATCH;
	case pat_cl_lower:
		return iswlower(wc) ? STEP : MISMATCH;
	case pat_cl_space:
		return iswspace(wc) ? STEP : MISMATCH;
	case pat_cl_digit:
		return iswdigit(wc) ? STEP : MISMATCH;
	default:
		return EINVAL;
	}
}

struct patthread
patfork(struct patthread *th)
{
	struct patthread ret = {0};

	ret.ins = th->ins;
	ret.pos = th->pos;
	ret.mat = vec_clone(th->mat);

	return ret;
}

size_t
patlex(struct pattok *tok, char const *src, size_t off)
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
patmatch(struct patmatch **res, struct patcontext *ctx, char const *str)
{
	int err = 0;
	int len = 0;
	size_t max = 0;
	struct patthread *fin;
	struct patthread *it;

	err = vec_ctor(fin);
	if (err) return err;

	while (str[ctx->pos]) {
		ctx->pos += len;

		err = patstep(&fin, ctx, str + ctx->pos);
		if (err) break;

		len = mblen(str + ctx->pos, strlen(str + ctx->pos));
		if (len == -1) {
			err = errno;
			break;
		}
	}

	if (err > 0) goto finally;

	if (!len(fin)) {
		err = -1;
		goto finally;
	}
	
	vec_foreach (it, fin)
		if (it->mat[0].ext > fin[max].mat[0].ext)
			max = it - fin;

	err = vec_copy(res, fin[max].mat);

finally:
	vec_foreach (it, fin) vec_free(it->mat);
	vec_free(fin);

	return err;
}

int
patstep(struct patthread **fin, struct patcontext *ctx, char const *str)
{
	int err = 0;
	size_t i = 0;

	for (i = 0; i < len(ctx->thr); ++i) {
		err = ip(ctx->thr + i)->op(ctx, ctx->thr + i, str);
		switch (err) {
		case 0:    break;
		case STEP: break;
		case MISMATCH:
			vec_free(ctx->thr[i].mat);
			vec_delete(&ctx->thr, i);
			--i;
			break;
		case MATCH:
			err = vec_append(fin, ctx->thr + i);
			if (err) return err;
			vec_delete(&ctx->thr, i);
			++ctx->nmat;
			break;
		default:
			return err;
		}
	}

	return len(ctx->thr) ? 0 : -1;
}

int
pat_compile(struct pattern *dest, char const *src)
{
	int err = 0;
	size_t off = 0;
	struct pattok tok = {0};
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
	struct patcontext ctx = {0};
	struct patthread *tmp = 0x0;
	struct patthread th = { .ins = pat->prog, .pos = 0, .mat = 0x0 };

	if (matv) vec_check(matv);

	err = vec_ctor(ctx.thr);
	if (err) return ENOMEM;

	err = vec_ctor(th.mat);
	if (err) goto finally;

	err = vec_append(&ctx.thr, &th);
	if (err) goto finally;

	err = patmatch(matv, &ctx, str);
	if (err) goto finally;

finally:
	vec_foreach(tmp, ctx.thr) vec_free(tmp->mat);
	vec_free(ctx.thr);
	return err;
}

void
pat_free(struct pattern *pat)
{
	vec_free(pat->prog);
}
