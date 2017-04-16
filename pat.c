#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "pat.h"
#include "vec.h"

enum pattype {
	LITERAL,
	OPT,	// ?
	STAR,	// *
	PLUS,	// +
	ALTER,	// |
	ANCH_BOL, // ^
	LPAREN,	// (
	RPAREN,	// )
};

enum patop {
	HALT = 0x0,
	CHAR = 0x1,
	FORK = 0x2,
	JUMP = 0x3,
	MARK = 0x4,
	SAVE = 0x5,
	CLSS = 0x6,
	REFR = 0x7,
};

enum patret {
	PAT_MISMATCH = -1,
	PAT_FORKED = -2,
	PAT_MATCH = -3,
};

typedef enum pattype pattype;
typedef enum patop patop;
typedef struct pattok pattok;
typedef struct patins patins;

struct patcontext {
	struct patthread *thr;
	size_t ind;
	size_t pos;
};

struct patins {
	enum patop op;
	uint64_t arg;
};

struct patthread {
	struct patins *ins;
	size_t pos;
	struct patmatch *mat;
};

struct pattok {
	size_t len;
	enum pattype type;
	wchar_t wc;
	int (*eval)(struct pattern *, struct pattok *, struct patins **);
};

static size_t patlex(struct pattok *, char const *, size_t);
static int pataddalt(struct pattern *, struct pattok *, struct patins **);
static int pataddbol(struct pattern *, struct pattok *, struct patins **);
static int pataddinit(struct pattern *);
static int pataddlpar(struct pattern *, struct pattok *, struct patins **);
static int pataddrpar(struct pattern *, struct pattok *, struct patins **);
static int pataddrep(struct pattern *, struct pattok *, struct patins **);
static int patdothread(struct patcontext *, char const *, struct patthread *);
static struct patthread patthread(struct patthread *);

int
pataddalt(struct pattern *pat, struct pattok *tok, struct patins **tmp)
{
	int err = 0;
	size_t offset = len(*tmp) + 2;
	struct patins ins = {0};

	if (offset < len(*tmp)) return EOVERFLOW;

	ins.op = FORK;
	ins.arg = offset;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	err = vec_join(&pat->prog, *tmp);
	if (err) return err;

	ins.op = JUMP;
	ins.arg = -1;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	vec_truncate(tmp, 0);

	return 0;
}

int
pataddbol(struct pattern *pat, struct pattok *tok, struct patins **tmp)
{
	vec_truncate(tmp, 0);
	return 0;
}

int
pataddinit(struct pattern *pat)
{
	struct patins insv[] = {
		[0] = { .op = FORK, .arg = 2 },
		[1] = { .op = JUMP, .arg = 4 },
		[2] = { .op = CLSS, .arg = 0 },
		[3] = { .op = FORK, .arg = 2 },
		[4] = { .op = MARK, .arg = 0 },
	};

	return vec_concat(&pat->prog, insv, sizeof insv / sizeof *insv);
}

int
pataddfini(struct pattern *pat)
{
	struct patins insv[] = {
		[0] = { .op = SAVE, .arg = 0 },
	};

	return vec_concat(&pat->prog, insv, sizeof insv / sizeof *insv);
}

int
pataddlit(struct pattern *pat, struct pattok *tok, struct patins **tmp)
{
	int err = 0;
	struct patins ins = {0};

	ins.op = CHAR;
	ins.arg = tok->wc;

	err = vec_append(tmp, &ins);
	if (err) return err;
	return 0;
}

int
pataddlpar(struct pattern *pat, struct pattok *tok, struct patins **tmp)
{
	int err = 0;
	struct patins ins = {0};

	err = vec_join(&pat->prog, *tmp);
	if (err) return err;

	ins.op = JUMP;
	ins.arg = -2;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	return 0;
}

int
pataddrep(struct pattern *pat, struct pattok *tok, struct patins **tmp)
{
	int err = 0;
	struct patins ins = {0};
	size_t again = len(pat->prog);
	size_t offset = tok->type != OPT ? 3 : 2;

	if (again < len(pat->prog)) return EOVERFLOW;

	if (!len(*tmp)) return EILSEQ;

	err = vec_concat(&pat->prog, *tmp, len(*tmp) - 1);
	if (err) return err;

	if (tok->type != PLUS) {
		ins.op = FORK;
		ins.arg = len(pat->prog) + offset;

		if (ins.arg < len(pat->prog)) return EOVERFLOW;

		err = vec_append(&pat->prog, &ins);
		if (err) return err;
		++again;
	}

	ins = (*tmp)[len(*tmp) - 1];
	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	if (tok->type != OPT) {
		ins.op = FORK;
		ins.arg = again;
		err = vec_append(&pat->prog, &ins);
		if (err) return err;
	}

	vec_truncate(tmp, 0);
	return 0;
}

int
pataddrpar(struct pattern *pat, struct pattok *tok, struct patins **tmp)
{
	int err = 0;
	size_t i = 0;
	uint64_t u = 0;
	struct patins ins = {0};
	struct patins *insp = 0x0;

	err = vec_join(&pat->prog, *tmp);
	if (err) return err;

	i = len(pat->prog);
	while (i --> 0) {
		insp = pat->prog + i;
		if (insp->op == JUMP && insp->arg > -3UL) {
			u = insp->arg;
			insp->arg = len(pat->prog) + 1;
			if (u == -2UL) break;
		}
	}

	ins.op = JUMP;
	ins.arg = len(pat->prog) + 2;
	if (ins.arg < len(pat->prog)) return EOVERFLOW;

	err = vec_append(&pat->prog, &ins);
	if (err) return EOVERFLOW;

	ins.op = JUMP;
	ins.arg = i + 1;
	if (ins.arg < i) return EOVERFLOW;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	vec_truncate(tmp, 0);

	return 0;
}

int
patcomp(struct pattern *dest, char const *src)
{
	int err = 0;
	size_t off = 0;
	struct pattok tok = {0};
	struct patins *tmp = 0x0;

	vec_ctor(tmp);
	if (!tmp) return ENOMEM;

	vec_ctor(dest->prog);
	if (!dest->prog) return ENOMEM;

	pataddinit(dest);

	while (src[off]) {
		off += patlex(&tok, src, off);
		err = tok.eval(dest, &tok, &tmp);
		if (err) goto finally;
	}

	pataddfini(dest);

finally:
	vec_free(tmp);
	if (err) vec_free(dest->prog);
	return err;
}

int
patdothread(struct patcontext *ctx, char const *str, struct patthread *th)
{
	int err = 0;
	int ret = 0;
	size_t i = 0;
	wchar_t wc = 0;
	struct patins *ins = 0x0;
	struct patthread new = {0};
	struct patmatch mat = {0};

again:
	ins = th->ins + th->pos++;

	switch (ins->op) {
	case HALT:
		ret = PAT_MATCH;
		break;
	case CHAR:
		err = mbtowc(&wc, str, strlen(str));
		if (err) return errno;
		if ((wchar_t)ins->arg != wc) ret = PAT_MISMATCH;
		break;
	case JUMP:
		th->pos = ins->arg;
		goto again;
	case FORK:
		new = patthread(th);
		if (!new.mat) return ENOMEM;
		new.pos = ins->arg;
		err = vec_append(&ctx->thr, &new);
		if (err) return err;
		++ctx->ind;
		goto again;
	case MARK:
		mat.off = ctx->pos;
		mat.ext = -1;
		err = vec_append(&th->mat, &mat);
		if (err) return err;
		goto again;
		break;
	case SAVE: 
		for (i = 0; i < len(th->mat); ++i)
			if (th->mat[i].ext == (size_t)-1) break;
		if (i == len(th->mat)) break;
		th->mat[i].ext = ctx->pos - th->mat[i].ext;
		goto again;
		break;
	case CLSS:
		assert(ins->arg == 0);
		if (!*str || *str == '\n') ret = PAT_MISMATCH;
		break;
	case REFR:
		break;
	}

	return ret;
}

int
patexec(patmatch_t **matv, char const *str, pattern_t const *pat)
{
	int err = 0;
	size_t i = 0;
	struct patcontext ctx = {0};
	struct patthread th = {0};
	struct patthread *tmp = {0};

	if (matv) vec_check(matv);

	err = vec_ctor(ctx.thr);
	if (err) goto nomem;

	err = vec_ctor(th.mat);
	if (err) goto nomem;

	th.ins = pat->prog;
	th.pos = 0;

	err = vec_append(&ctx.thr, &th);
	if (err) return err;

	do for (ctx.ind = i = 0; i < len(ctx.thr); i = ++ctx.ind) {
		err = patdothread(&ctx, str + ctx.pos, ctx.thr + i);

		if (err == PAT_MISMATCH) {
			vec_free(ctx.thr[ctx.ind].mat);
			vec_delete(&ctx.thr, ctx.ind--);
		}
		else if (err == PAT_FORKED) ++ctx.ind;
		else if (err == PAT_MATCH) goto done; 
		else if (err > 0) goto fail;

	} while (str[++ctx.pos]);

done:

	th = ctx.thr[i];
	vec_map (tmp, ctx.thr) vec_free(tmp->mat);
	vec_free(ctx.thr);

	if (matv) err = vec_join(matv, th.mat);
	return err;

nomem:
	err = ENOMEM;
fail:
	vec_map (tmp, ctx.thr) vec_free(tmp->mat);
	vec_free(ctx.thr);
	return err;
}

void
patfree(pattern_t *pat)
{
	vec_free(pat->prog);
}

size_t
patlex(struct pattok *tok, char const *src, size_t off)
{
	size_t ret;
	ret = tok->len = 1;
	switch (src[off]) {
	case '?':
		tok->type = OPT;
		tok->eval = pataddrep;
		break;
	case '*':
		tok->type = STAR;
		tok->eval = pataddrep;
		break;
	case '+':
		tok->type = PLUS;
		tok->eval = pataddrep;
		break;
	case '(':
		tok->type = LPAREN;
		tok->eval = pataddlpar;
		break;
	case ')':
		tok->type = RPAREN;
		tok->eval = pataddrpar;
		break;
	case '|':
		tok->type = ALTER;
		tok->eval = pataddalt;
		break;
	case '^':
		if (off) goto literal;
		tok->type = ANCH_BOL;
		tok->eval = pataddbol;
		break;
	case '\\':
		goto esc;
	default:
literal:
		tok->type = LITERAL;
		tok->eval = pataddlit;
		ret = tok->len = mbtowc(&tok->wc, src + off, strlen(src + off));
	}

	return ret;

esc:
	abort();
	switch (src[1]) {
	default:
		tok->type = LITERAL;
		tok->len = 1 + mblen(src, strlen(src + 1));
	}

	return ret;
}

int
patmatch(patmatch_t **matv, char const *str, char const *pat, long opts)
{
	return -1;
}

struct patthread
patthread(struct patthread *th)
{
	struct patthread ret = {0};

	ret.ins = th->ins;
	ret.pos = th->pos;
	ret.mat = vec_clone(&th->mat);

	return ret;
}
