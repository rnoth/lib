#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "pat.h"
#include "util.h"
#include "vec.h"

enum patclass {
	DOT = 0x0,
	ALPHA = 0x1,
	UPPER = 0x2,
	LOWER = 0x3,
	DIGIT = 0x4,
	SPACE = 0x5,
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

enum pattype {
	LITER,
	QMARK,
	STAR,
	PLUS,
	ALTER,
	BOL,
	EOL,
	LPAR,
	RPAR,
};

enum patret {
	MISMATCH = -1,
	MATCH = -2,
};

struct patcontext {
	size_t		  pos;
	size_t		  ind;
	size_t		  nmat;
	struct patthread *thr;
};

struct patins {
	enum	 patop	op;
	uint64_t	arg;
};

struct patthread {
	struct patmatch *mat;
	struct patins 	*ins;
	size_t		 pos;
};

struct pattok {
	size_t		 len;
	enum pattype	 type;
	wchar_t		 wc;
};

static int pataddalt(struct pattern *, struct pattok *, struct patins **);
static int pataddbol(struct pattern *, struct pattok *, struct patins **);
static int pataddlit(struct pattern *, struct pattok *, struct patins **);
static int pataddlpar(struct pattern *, struct pattok *, struct patins **);
static int pataddrpar(struct pattern *, struct pattok *, struct patins **);
static int pataddrep(struct pattern *, struct pattok *, struct patins **);

static int pataddinit(struct pattern *);
static int pataddfini(struct pattern *, struct patins **);

static size_t patlex(struct pattok *, char const *, size_t);
static int patmatch(struct patmatch **, struct patcontext *, char const *);
static int patstep(struct patthread **, struct patcontext *, char const *);
static int patstepthread(struct patcontext *, char const *, struct patthread *);
static struct patthread patfork(struct patthread *);

static int (* const patadd[])(struct pattern *, struct pattok *, struct patins **) = {
	[LITER]	= pataddlit,
	[QMARK]	= pataddrep,
	[STAR]	= pataddrep,
	[PLUS]	= pataddrep,
	[ALTER] = pataddalt,
	[BOL]	= pataddbol,
	[EOL]	= 0x0,
	[LPAR]	= pataddlpar,
	[RPAR]	= pataddrpar,
};

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
	struct patins ins = { .op = SAVE, .arg = 0 };

	err = vec_join(&pat->prog, *bufp);
	if (err) return err;

	vec_truncate(bufp, 0);

	return vec_append(&pat->prog, &ins);
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

	return vec_append(&pat->prog, &ins);
}

int
pataddrep(struct pattern *pat, struct pattok *tok, struct patins **bufp)
{
	int err = 0;
	struct patins ins = {0};
	size_t again = len(pat->prog);
	size_t offset = tok->type != QMARK ? 3 : 2;

	if (!len(*bufp)) return 0;

	if (addz_overflows(len(pat->prog), len(*bufp) + offset))
		return EOVERFLOW;

	err = vec_concat(&pat->prog, *bufp, len(*bufp) - 1);
	if (err) return err;

	if (tok->type != PLUS) {
		ins.op = FORK;
		ins.arg = len(pat->prog) + offset;

		err = vec_append(&pat->prog, &ins);
		if (err) return err;

		++again;
	}

	ins = (*bufp)[len(*bufp) - 1];

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	if (tok->type != QMARK) {
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

	for (i = len(pat->prog); p = pat->prog + i, i --> 0;) {
		if (p->op == JUMP && p->arg > -3UL) {
			arg = p->arg;
			p->arg = len(pat->prog) + 1;
			if (p->arg > len(pat->prog)) return EOVERFLOW;
			if (arg == -2UL) break;
		}
	}

	ins.op = JUMP;
	ins.arg = len(pat->prog) + 2;
	if (ins.arg < len(pat->prog)) return EOVERFLOW;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	ins.op = JUMP;
	ins.arg = i + 1;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	return 0;
}

struct patthread
patfork(struct patthread *th)
{
	struct patthread ret = {0};

	ret.ins = th->ins;
	ret.pos = th->pos;
	ret.mat = vec_clone(&th->mat);

	return ret;
}

size_t
patlex(struct pattok *tok, char const *src, size_t off)
{
	size_t ret;
	ret = tok->len = 1;
	switch (src[off]) {
	case '?':
		tok->type = QMARK;
		break;
	case '*':
		tok->type = STAR;
		break;
	case '+':
		tok->type = PLUS;
		break;
	case '(':
		tok->type = LPAR;
		break;
	case ')':
		tok->type = RPAR;
		break;
	case '|':
		tok->type = ALTER;
		break;
	case '^':
		if (off) goto literal;
		tok->type = BOL;
		break;
	case '\\':
		goto esc;
	default:
literal:
		tok->type = LITER;
		ret = tok->len = mbtowc(&tok->wc, src + off, strlen(src + off));
	}

	return ret;

esc:
	abort();
	switch (src[1]) {
	default:
		tok->type = LITER;
		tok->len = 1 + mblen(src, strlen(src + 1));
	}

	return ret;
}

int
patmatch(struct patmatch **res, struct patcontext *ctx, char const *str)
{
	int err = 0;
	int len = 0;
	size_t cur = 0;
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

	if (err > 0) {
		vec_free(fin);
		return err;
	}
	
	vec_foreach (it, fin)
		if (it->mat[0].ext > fin[cur].mat[0].ext)
			cur = it - fin;

	vec_copy(res, fin[cur].mat);

	return 0;
}

int
patstep(struct patthread **fin, struct patcontext *ctx, char const *str)
{
	int err = 0;
	size_t i = 0;

	for (i = 0; i < len(ctx->thr); ++i) {
		err = patstepthread(ctx, str, ctx->thr + i);

		switch (err) {
		case 0:
			break;
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
patstepthread(struct patcontext *ctx, char const *str, struct patthread *th)
{
	int err = 0;
	int len = 0;
	int ret = 0;
	wchar_t wc = 0;
	struct patins *ins = 0x0;
	struct patthread new = {0};
	struct patmatch mat = {0};

again:
	ins = th->ins + th->pos++;

	switch (ins->op) {
	case HALT:
		ret = MATCH;
		break;

	case CHAR:
		len = mbtowc(&wc, str, strlen(str));
		if (len < 0) return errno;

		if ((wchar_t)ins->arg != wc) ret = MISMATCH;
		break;

	case JUMP:
		th->pos = ins->arg;
		goto again;

	case FORK:
		new = patfork(th);
		if (!new.mat) return ENOMEM;

		new.pos = ins->arg;

		err = vec_insert(&ctx->thr, &new, th - ctx->thr + 1);
		if (err) return err;

		goto again;

	case MARK:
		mat.off = ctx->pos;
		mat.ext = -1;

		err = vec_append(&th->mat, &mat);
		if (err) return err;

		goto again;

	case SAVE: 
		th->mat[ins->arg].ext = ctx->pos - th->mat[ins->arg].off;

		goto again;

	case CLSS:
		if (!*str || *str == '\n') ret = MISMATCH;
		break;

	case REFR:
		break;
	}

	return ret;
}

int
patcomp(struct pattern *dest, char const *src)
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
pateval(patmatch_t **matv, char const *str, char const *pat, long opts)
{
	return -1;
}

int
patexec(patmatch_t **matv, char const *str, pattern_t const *pat)
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
patfree(pattern_t *pat)
{
	vec_free(pat->prog);
}
